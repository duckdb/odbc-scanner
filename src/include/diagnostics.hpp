#pragma once

#include "duckdb_extension.h"

#include <sql.h>
#include <sqlext.h>
#include <string>

namespace odbcscanner {

std::string ReadDiagnostics(SQLHANDLE handle, SQLSMALLINT handle_type);

std::string ReadDiagnosticsCode(SQLHANDLE handle, SQLSMALLINT handle_type);

} // namespace odbcscanner
