#pragma once

#include "capi_pointers.hpp"
#include "duckdb_extension.h"

#include <memory>
#include <sql.h>
#include <sqlext.h>
#include <string>
#include <vector>

namespace odbcscanner {

void SetOdbcParam(const std::string &query, HSTMT hstmt, duckdb_value value, SQLSMALLINT param_idx,
                  std::vector<void *> &holder);

ValuePtr ExtractInputParam(duckdb_data_chunk input, idx_t col_idx);

} // namespace odbcscanner
