#include "connection.hpp"

#include <cstdint>
#include <vector>

#include "diagnostics.hpp"
#include "scanner_exception.hpp"
#include "widechar.hpp"

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

OdbcConnection::OdbcConnection(const std::string &url) {
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

	{
		auto wurl = WideChar::Widen(url.data(), url.length());
		SQLRETURN ret = SQLDriverConnectW(dbc, nullptr, wurl.data(), wurl.length<SQLSMALLINT>(), nullptr, 0, nullptr,
		                                  SQL_DRIVER_NOPROMPT);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(dbc, SQL_HANDLE_DBC);
			throw ScannerException("'SQLDriverConnect' failed, url: '" + url + "', return: " + std::to_string(ret) +
			                       ", diagnostics: '" + diag + "'");
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
			throw ScannerException("'SQLGetInfo' failed for SQL_DBMS_NAME, url: '" + url +
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
			throw ScannerException("'SQLGetInfo' failed for SQL_DRIVER_NAME, url: '" + url +
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

} // namespace odbcscanner
