#pragma once

#include "duckdb_extension.h"

#include <sql.h>
#include <sqlext.h>
#include <string>

namespace odbcscanner {

struct Diagnostics {
	static std::string Read(SQLHANDLE handle, SQLSMALLINT handle_type);

	static std::string ReadCode(SQLHANDLE handle, SQLSMALLINT handle_type);
};

} // namespace odbcscanner
