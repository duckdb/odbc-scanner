#pragma once

#include "duckdb_extension.h"

#include <sql.h>
#include <sqlext.h>
#include <string>

namespace odbcscanner {

void CheckChunkRowsCount(duckdb_data_chunk chunk, idx_t req_size = 1);

std::string ExtractStringFromChunk(duckdb_data_chunk chunk, idx_t col_idx, idx_t row_idx = 0);

int64_t ExtractInt64FromChunk(duckdb_data_chunk chunk, idx_t col_idx, idx_t row_idx = 0);

template <typename T>
T *ExtractPtrFromChunk(duckdb_data_chunk chunk, idx_t col_idx, idx_t row_idx = 0) {
	int64_t num = ExtractInt64FromChunk(chunk, col_idx, row_idx);
	return reinterpret_cast<T *>(num);
}

std::string ReadDiagnostics(SQLHANDLE handle, SQLSMALLINT handle_type);

} // namespace odbcscanner
