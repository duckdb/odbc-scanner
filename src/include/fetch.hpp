#pragma once

#include "duckdb_extension.h"

#include <sql.h>
#include <sqlext.h>
#include <string>

namespace odbcscanner {

void FetchIntoVector(SQLSMALLINT odbc_ctype, const std::string &query, HSTMT hstmt, SQLSMALLINT col_idx,
                     duckdb_vector vec, idx_t row_idx);

} // namespace odbcscanner