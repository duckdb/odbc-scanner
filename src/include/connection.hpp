#pragma once

#include <cstdint>
#include <string>

#include "odbc_api.hpp"

namespace odbcscanner {

enum class DbmsDriver {
	ORACLE,
	MSSQL,
	DB2,

	MARIADB,
	MYSQL,
	POSTGRESQL,

	SNOWFLAKE,
	SPARK,
	CLICKHOUSE,
	FLIGTHSQL,

	GENERIC
};

struct OdbcConnection {
	SQLHANDLE env = nullptr;
	SQLHANDLE dbc = nullptr;
	DbmsDriver driver;

	OdbcConnection(const std::string &url);
	~OdbcConnection() noexcept;

	OdbcConnection(OdbcConnection &other) = delete;
	OdbcConnection(OdbcConnection &&other) = delete;

	OdbcConnection &operator=(const OdbcConnection &) = delete;
	OdbcConnection &operator=(OdbcConnection &&other) = delete;
};

} // namespace odbcscanner
