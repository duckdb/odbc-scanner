#pragma once

#include "params.hpp"
#include "types/types.hpp"

namespace odbcscanner {

void SetNullParam(const std::string &query, HSTMT hstmt, SQLSMALLINT param_idx);

void SetNullResult(duckdb_vector vec, idx_t row_idx);

} // namespace odbcscanner
