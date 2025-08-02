#pragma once

#include <sql.h>
#include <sqlext.h>
#include <string>

namespace odbcscanner {

struct OdbcConnection {
	SQLHANDLE env = nullptr;
	SQLHANDLE dbc = nullptr;

	OdbcConnection(const std::string &url);
	~OdbcConnection() noexcept;

	OdbcConnection &operator=(const OdbcConnection &) = delete;
	OdbcConnection &operator=(OdbcConnection &&other) = delete;
};

} // namespace odbcscanner
