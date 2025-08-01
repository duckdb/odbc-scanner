#pragma once

#include <cstdint>
#include <sql.h>
#include <sqlext.h>
#include <string>

namespace odbcscanner {

struct OdbcConnection {
	SQLHANDLE env = nullptr;
	SQLHANDLE dbc = nullptr;

	OdbcConnection(const std::string &url) {
		SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
		SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(static_cast<uintptr_t>(SQL_OV_ODBC3)),
		              0);
		SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
		SQLDriverConnect(dbc, nullptr, reinterpret_cast<SQLCHAR *>(const_cast<char *>(url.c_str())),
		                 static_cast<SQLSMALLINT>(url.length()), nullptr, 0, nullptr, SQL_DRIVER_COMPLETE_REQUIRED);
	}

	OdbcConnection &operator=(const OdbcConnection &) = delete;
	OdbcConnection &operator=(OdbcConnection &&other) = delete;

	~OdbcConnection() {
		SQLDisconnect(dbc);
		SQLFreeHandle(SQL_HANDLE_DBC, dbc);
		SQLFreeHandle(SQL_HANDLE_ENV, env);
	}
};

} // namespace odbcscanner
