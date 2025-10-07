#pragma once

#include <cstdint>
#include <string>

#include <sql.h>
#include <sqlext.h>

namespace odbcscanner {

struct OdbcConnection {
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
