#pragma once

#include <string>

#include "duckdb_extension_api.hpp"
#include "odbc_api.hpp"

namespace odbcscanner {

struct Diagnostics {
	static std::string Read(SQLHANDLE handle, SQLSMALLINT handle_type);

	static std::string ReadCode(SQLHANDLE handle, SQLSMALLINT handle_type);
};

} // namespace odbcscanner
