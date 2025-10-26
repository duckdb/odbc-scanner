#pragma once

#include <cstdint>
#include <string>

#include "odbc_api.hpp"

namespace odbcscanner {

struct OdbcConnection {
	static const std::string MSSQL_DBMS_NAME;

	SQLHANDLE env = nullptr;
	SQLHANDLE dbc = nullptr;
	std::string dbms_name;

	OdbcConnection(const std::string &url);
	~OdbcConnection() noexcept;

	OdbcConnection(OdbcConnection &other) = delete;
	OdbcConnection(OdbcConnection &&other) = delete;

	OdbcConnection &operator=(const OdbcConnection &) = delete;
	OdbcConnection &operator=(OdbcConnection &&other) = delete;
};

} // namespace odbcscanner
