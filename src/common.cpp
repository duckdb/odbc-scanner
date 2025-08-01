#include "common.hpp"

#include "scanner_exception.hpp"

#include <string>

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

void CheckChunkRowsCount(duckdb_data_chunk chunk, idx_t req_size) {
	idx_t count = duckdb_data_chunk_get_size(chunk);
	if (count != 1) {
		throw ScannerException("Invalid input row count, value: " + std::to_string(count) +
		                       ", must be: " + std::to_string(req_size));
	}
}

std::string ExtractStringFromChunk(duckdb_data_chunk chunk, idx_t col_idx, idx_t row_idx) {
	std::string err_prefix = "Cannot extract string from chunk, column: " + std::to_string(col_idx) +
	                         ", row: " + std::to_string(row_idx) + ": ";

	duckdb_vector vec = duckdb_data_chunk_get_vector(chunk, col_idx);
	if (vec == nullptr) {
		throw ScannerException(err_prefix + "vector is NULL");
	}

	idx_t rows_count = duckdb_data_chunk_get_size(chunk);
	if (row_idx >= rows_count) {
		throw ScannerException(err_prefix + "invalid row index, rows count: " + std::to_string(rows_count));
	}

	duckdb_string_t *str_data = reinterpret_cast<duckdb_string_t *>(duckdb_vector_get_data(vec));
	duckdb_string_t str_t = str_data[row_idx];

	const char *cstr = duckdb_string_t_data(&str_t);
	uint32_t len = duckdb_string_t_length(str_t);

	return std::string(cstr, len);
}

} // namespace odbcscanner
