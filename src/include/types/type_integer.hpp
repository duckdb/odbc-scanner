#pragma once

#include <cstdint>

#include "params.hpp"
#include "types/types.hpp"

namespace odbcscanner {

std::pair<int32_t, bool> ExtractIntegerFunctionArg(duckdb_data_chunk chunk, idx_t col_idx);

void AddIntegerResultColumn(duckdb_bind_info info, const std::string &name);

ScannerParam ExtractIntegerNotNullParam(duckdb_vector vec);

void SetIntegerParam(const std::string &query, HSTMT hstmt, ScannerParam &param, SQLSMALLINT param_idx);

std::pair<int32_t, bool> FetchInteger(const std::string &query, HSTMT hstmt, SQLSMALLINT col_idx);

void SetIntegerResult(duckdb_vector vec, idx_t row_idx, int32_t value);

} // namespace odbcscanner
