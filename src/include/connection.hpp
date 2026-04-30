#pragma once

#include <cstdint>
#include <string>

#include "duckdb_extension_api.hpp"
#include "odbc_api.hpp"

namespace odbcscanner {

enum class DbmsDriver {
	ORACLE,
	MSSQL,
	DB2,

	MARIADB,
	MYSQL,
	POSTGRESQL,
	FIREBIRD,

	SNOWFLAKE,
	SPARK,
	CLICKHOUSE,
	FLIGTHSQL,

	GENERIC
};

struct ExtractedConnection;

struct OdbcConnection {
	SQLHANDLE env = nullptr;
	SQLHANDLE dbc = nullptr;
	DbmsDriver driver;

	// access_token is optional; when non-empty it is passed to the SQL Server ODBC driver via
	// SQL_COPT_SS_ACCESS_TOKEN (connection attribute 1256) before the connection is opened.
	// The token must be a raw OAuth/AAD bearer token string (UTF-8).  The driver requires it
	// in UTF-16LE form preceded by a 4-byte byte-length field (the ACCESSTOKEN struct from
	// msodbcsql.h); that encoding is handled internally.
	OdbcConnection(const std::string &url, const std::string &access_token = "");
	~OdbcConnection() noexcept;

	OdbcConnection(OdbcConnection &other) = delete;
	OdbcConnection(OdbcConnection &&other) = delete;

	OdbcConnection &operator=(const OdbcConnection &) = delete;
	OdbcConnection &operator=(OdbcConnection &&other) = delete;

	static ExtractedConnection ExtractOrOpen(const std::string &function_name, duckdb_value conn_id_or_str_val);
};

struct ExtractedConnection {
	int64_t id = -1;
	std::unique_ptr<OdbcConnection> ptr;
	bool must_be_closed = false;

	ExtractedConnection(int64_t id_in, std::unique_ptr<OdbcConnection> ptr_in, bool must_be_closed_in)
	    : id(id_in), ptr(std::move(ptr_in)), must_be_closed(std::move(must_be_closed_in)) {
	}
};

} // namespace odbcscanner
