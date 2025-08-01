#pragma once

#include "duckdb_extension.h"

#include <string>

namespace odbcscanner {

void CheckChunkRowsCount(duckdb_data_chunk chunk, idx_t req_size = 1);

std::string ExtractStringFromChunk(duckdb_data_chunk chunk, idx_t col_idx, idx_t row_idx = 0);

} // namespace odbcscanner
