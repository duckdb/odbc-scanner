#include "connection.hpp"

#include <cstdint>
#include <cstring>
#include <regex>
#include <vector>

#include "capi_pointers.hpp"
#include "diagnostics.hpp"
#include "make_unique.hpp"
#include "registries.hpp"
#include "scanner_exception.hpp"
#include "widechar.hpp"

// SQL_COPT_SS_ACCESS_TOKEN is a Microsoft SQL Server-specific pre-connect attribute
// (value 1256, defined in msodbcsql.h).  We define it here so that the Microsoft
// ODBC header is not a build dependency on non-Windows platforms.
#ifndef SQL_COPT_SS_ACCESS_TOKEN
#define SQL_COPT_SS_ACCESS_TOKEN 1256
#endif

namespace odbcscanner {

static DbmsDriver ResolveDbmsDriver(const std::string &dbms_name, const std::string &driver_name) {
	if (dbms_name == "Oracle") {
		return DbmsDriver::ORACLE;
	} else if (dbms_name == "Microsoft SQL Server") {
		return DbmsDriver::MSSQL;
	} else if (dbms_name.rfind("DB2/", 0) == 0) {
		return DbmsDriver::DB2;

	} else if (dbms_name == "MariaDB") {
		return DbmsDriver::MARIADB;
	} else if (dbms_name == "MySQL") {
		return DbmsDriver::MYSQL;
	} else if (dbms_name == "Firebird") {
		return DbmsDriver::FIREBIRD;

	} else if (dbms_name == "Snowflake") {
		return DbmsDriver::SNOWFLAKE;
	} else if (dbms_name == "Spark SQL") {
		return DbmsDriver::SPARK;
	} else if (dbms_name == "ClickHouse") {
		return DbmsDriver::CLICKHOUSE;
	} else if (driver_name == "Arrow Flight ODBC Driver") {
		return DbmsDriver::FLIGTHSQL;
	} else {
		return DbmsDriver::GENERIC;
	}
}

// Note: we don't want to parse the connection string here,
// if UID or PWD is escaped (starts with '{') - we are filtering out
// everything until the end of the connection string
static std::string FilterPwd(const std::string url) {
	std::regex uid_pattern("UID=(\\{.*|[^;]+)", std::regex_constants::icase);
	auto uid_filtered = std::regex_replace(url, uid_pattern, "UID=***");
	std::regex pwd_pattern("PWD=(\\{.*|[^;]+)", std::regex_constants::icase);
	return std::regex_replace(uid_filtered, pwd_pattern, "PWD=***");
}

OdbcConnection::OdbcConnection(const std::string &url, const std::string &access_token) {
	{
		SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
		if (!SQL_SUCCEEDED(ret)) {
			throw ScannerException("'SQLAllocHandle' failed for ENV handle, return: " + std::to_string(ret));
		}
	}

	{
		SQLRETURN ret = SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION,
		                              reinterpret_cast<SQLPOINTER>(static_cast<uintptr_t>(SQL_OV_ODBC3)), 0);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(env, SQL_HANDLE_ENV);
			throw ScannerException("'SQLSetEnvAttr' failed, return: " + std::to_string(ret) + ", diagnostics: '" +
			                       diag + "'");
		}
	}

	{
		SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
		if (!SQL_SUCCEEDED(ret)) {
			throw ScannerException("'SQLAllocHandle' failed for DBC handle, return: " + std::to_string(ret));
		}
	}

	// Set SQL_COPT_SS_ACCESS_TOKEN before connecting when an access token is provided.
	// The Microsoft ODBC Driver for SQL Server requires the attribute value to point to an
	// ACCESSTOKEN structure: a 4-byte unsigned integer holding the byte length of the token
	// data, immediately followed by that many bytes of the token encoded in UTF-16LE.
	// We build the structure in a local buffer that lives until after SQLDriverConnectW.
	std::vector<unsigned char> access_token_buf;
	if (!access_token.empty()) {
		auto wtoken = WideChar::Widen(access_token.data(), access_token.length());
		// wtoken.length<size_t>() = number of SQLWCHAR code units, WITHOUT the null terminator
		size_t wtoken_len = wtoken.length<size_t>();
		uint32_t token_byte_len = static_cast<uint32_t>(wtoken_len * sizeof(SQLWCHAR));

		// Allocate: 4-byte size field + token bytes (no null terminator)
		access_token_buf.resize(sizeof(uint32_t) + token_byte_len);
		memcpy(access_token_buf.data(), &token_byte_len, sizeof(uint32_t));
		memcpy(access_token_buf.data() + sizeof(uint32_t), wtoken.data(), token_byte_len);

		SQLRETURN ret =
		    SQLSetConnectAttrW(dbc, SQL_COPT_SS_ACCESS_TOKEN,
		                       reinterpret_cast<SQLPOINTER>(access_token_buf.data()), SQL_IS_POINTER);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(dbc, SQL_HANDLE_DBC);
			throw ScannerException("'SQLSetConnectAttrW' failed for SQL_COPT_SS_ACCESS_TOKEN, connection string: '" +
			                       FilterPwd(url) + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag +
			                       "'");
		}
	}

	{
		auto wurl = WideChar::Widen(url.data(), url.length());
		SQLRETURN ret = SQLDriverConnectW(dbc, nullptr, wurl.data(), wurl.length<SQLSMALLINT>(), nullptr, 0, nullptr,
		                                  SQL_DRIVER_NOPROMPT);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(dbc, SQL_HANDLE_DBC);
			throw ScannerException("'SQLDriverConnect' failed, connection string: '" + FilterPwd(url) +
			                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
		}
	}

	std::string dbms_name;
	{
		std::vector<char> buf;
		buf.resize(256);
		SQLSMALLINT len = 0;
		SQLRETURN ret = SQLGetInfo(dbc, SQL_DBMS_NAME, buf.data(), static_cast<SQLSMALLINT>(buf.size()), &len);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(dbc, SQL_HANDLE_DBC);
			throw ScannerException("'SQLGetInfo' failed for SQL_DBMS_NAME, connection string: '" + FilterPwd(url) +
			                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
		}
		dbms_name = std::string(buf.data(), len);
	}

	std::string driver_name;
	{
		std::vector<char> buf;
		buf.resize(256);
		SQLSMALLINT len = 0;
		SQLRETURN ret = SQLGetInfo(dbc, SQL_DRIVER_NAME, buf.data(), static_cast<SQLSMALLINT>(buf.size()), &len);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(dbc, SQL_HANDLE_DBC);
			throw ScannerException("'SQLGetInfo' failed for SQL_DRIVER_NAME, connection string: '" + FilterPwd(url) +
			                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
		}
		driver_name = std::string(buf.data(), len);
	}

	this->driver = ResolveDbmsDriver(dbms_name, driver_name);
}

OdbcConnection::~OdbcConnection() noexcept {
	SQLDisconnect(dbc);
	SQLFreeHandle(SQL_HANDLE_DBC, dbc);
	SQLFreeHandle(SQL_HANDLE_ENV, env);
}

ExtractedConnection OdbcConnection::ExtractOrOpen(const std::string &function_name, duckdb_value conn_id_or_str_val) {
	if (duckdb_is_null_value(conn_id_or_str_val)) {
		throw ScannerException("'" + function_name + "' error: specified ODBC connection must be not NULL");
	}

	duckdb_logical_type ltype = duckdb_get_value_type(conn_id_or_str_val);
	duckdb_type type_id = duckdb_get_type_id(ltype);

	int64_t conn_id = -1;
	bool must_be_closed = false;
	if (type_id == DUCKDB_TYPE_VARCHAR) {
		auto conn_cstr = VarcharPtr(duckdb_get_varchar(conn_id_or_str_val), VarcharDeleter);
		if (conn_cstr == nullptr) {
			throw ScannerException("'" + function_name + "' error: extracted ODBC connection must be not NULL");
		}
		std::string conn_str(conn_cstr.get());
		auto oc_ptr = std_make_unique<OdbcConnection>(conn_str);
		conn_id = ConnectionsRegistry::Add(std::move(oc_ptr));
		must_be_closed = true;
	} else if (type_id == DUCKDB_TYPE_BIGINT) {
		conn_id = duckdb_get_int64(conn_id_or_str_val);
	} else {
		throw ScannerException("'" + function_name +
		                       "' error: invalid first argument specified, type ID: " + std::to_string(type_id) +
		                       ", must be either 'BIGINT' as an output of 'odbc_connect()' (example: 'FROM "
		                       "odbc_query(getvariable(\'conn\'), ...)')" +
		                       " or an ODBC connection string (for one-off queries)");
	}

	auto conn_ptr = ConnectionsRegistry::Remove(conn_id);
	if (conn_ptr.get() == nullptr) {
		throw ScannerException("'" + function_name +
		                       "' error: ODBC connection not found on bind, id: " + std::to_string(conn_id));
	}

	return ExtractedConnection(conn_id, std::move(conn_ptr), must_be_closed);
}

} // namespace odbcscanner
