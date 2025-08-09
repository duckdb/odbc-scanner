#pragma once

#include <cstdint>

#include "params.hpp"
#include "types/types.hpp"

namespace odbcscanner {

std::pair<int64_t, bool> ExtractBigIntFunctionArg(duckdb_data_chunk chunk, idx_t col_idx);

} // namespace odbcscanner
