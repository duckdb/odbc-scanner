#pragma once

#include "duckdb_extension.h"
#include "types/types.hpp"

#include <sql.h>
#include <sqlext.h>
#include <string>

namespace odbcscanner {

void FetchIntoVector(const std::string &query, HSTMT hstmt, SQLSMALLINT col_idx, const OdbcType &odbc_type,
                     duckdb_vector vec, idx_t row_idx);

} // namespace odbcscanner
