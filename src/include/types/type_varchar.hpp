#pragma once

#include "params.hpp"
#include "types/types.hpp"

namespace odbcscanner {

std::pair<std::string, bool> ExtractVarcharFunctionArg(duckdb_data_chunk chunk, idx_t col_idx);

void AddVarcharResultColumn(duckdb_bind_info info, const std::string &name);

ScannerParam ExtractVarcharNotNullInputParam(duckdb_vector vec);

void SetVarcharParam(const std::string &query, HSTMT hstmt, ScannerParam &param, SQLSMALLINT param_idx);

std::pair<std::string, bool> FetchVarchar(const std::string &query, HSTMT hstmt, SQLSMALLINT col_idx);

void SetVarcharResult(duckdb_vector vec, idx_t row_idx, const std::string &value);

} // namespace odbcscanner
