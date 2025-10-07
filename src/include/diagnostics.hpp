#pragma once

#include "duckdb_extension.h"

#include <string>

#include <sql.h>
#include <sqlext.h>

namespace odbcscanner {

struct Diagnostics {
	static std::string Read(SQLHANDLE handle, SQLSMALLINT handle_type);

	static std::string ReadCode(SQLHANDLE handle, SQLSMALLINT handle_type);
};

} // namespace odbcscanner
