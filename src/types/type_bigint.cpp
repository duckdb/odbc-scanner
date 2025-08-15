#include "scanner_exception.hpp"
#include "types/type_integer.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

std::pair<int64_t, bool> ExtractBigIntFunctionArg(duckdb_data_chunk chunk, idx_t col_idx) {
	idx_t col_count = duckdb_data_chunk_get_column_count(chunk);
	if (col_idx >= col_count) {
		throw ScannerException("Cannot extract BIGINT function argument: column not found, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	duckdb_vector vec = duckdb_data_chunk_get_vector(chunk, col_idx);
	if (vec == nullptr) {
		throw ScannerException("Cannot extract BIGINT function argument: vector is NULL, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	idx_t rows_count = duckdb_data_chunk_get_size(chunk);
	if (rows_count == 0) {
		throw ScannerException("Cannot extract BIGINT function argument: vector contains no rows, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	uint64_t *validity = duckdb_vector_get_validity(vec);
	if (validity != nullptr && !duckdb_validity_row_is_valid(validity, 0)) {
		return std::make_pair(-1, true);
	}

	int64_t *int64_data = reinterpret_cast<int64_t *>(duckdb_vector_get_data(vec));
	int64_t res = int64_data[0];
	return std::make_pair(res, false);
}

} // namespace odbcscanner
