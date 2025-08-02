#include "odbc_connection.hpp"

#include "common.hpp"
#include "scanner_exception.hpp"

#include <cstdint>

namespace odbcscanner {

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
			std::string diag = ReadDiagnostics(env, SQL_HANDLE_ENV);
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
		SQLRETURN ret =
		    SQLDriverConnect(dbc, nullptr, reinterpret_cast<SQLCHAR *>(const_cast<char *>(url.c_str())),
		                     static_cast<SQLSMALLINT>(url.length()), nullptr, 0, nullptr, SQL_DRIVER_COMPLETE_REQUIRED);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = ReadDiagnostics(dbc, SQL_HANDLE_DBC);
			throw ScannerException("'SQLDriverConnect' failed, url: '" + url + "', return: " + std::to_string(ret) +
			                       ", diagnostics: '" + diag + "'");
		}
	}
}

OdbcConnection::~OdbcConnection() noexcept {
	SQLDisconnect(dbc);
	SQLFreeHandle(SQL_HANDLE_DBC, dbc);
	SQLFreeHandle(SQL_HANDLE_ENV, env);
}

} // namespace odbcscanner