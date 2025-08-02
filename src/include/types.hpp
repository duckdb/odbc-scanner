#pragma once

#include "duckdb_extension.h"

#include <sql.h>
#include <sqlext.h>

namespace odbcscanner {

duckdb_type OdbcCTypeToDuckType(SQLLEN odbc_ctype);

} // namespace odbcscanner
