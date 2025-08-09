#include "params.hpp"

#include "scanner_exception.hpp"
#include "types/type_integer.hpp"
#include "types/type_null.hpp"
#include "types/type_varchar.hpp"
#include "widechar.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

ScannerParam::ScannerParam() : type_id(DUCKDB_TYPE_SQLNULL) {
}

ScannerParam::ScannerParam(int32_t value) : type_id(DUCKDB_TYPE_INTEGER), int32(value), len_bytes(sizeof(value)) {
}

ScannerParam::ScannerParam(int64_t value) : type_id(DUCKDB_TYPE_BIGINT), int64(value), len_bytes(sizeof(value)) {
}

ScannerParam::ScannerParam(std::string value)
    : type_id(DUCKDB_TYPE_VARCHAR), str(std::move(value)), wstr(utf8_to_utf16_lenient(str.data(), str.length())),
      len_bytes(wstr.length<SQLLEN>() * sizeof(SQLWCHAR)) {
}

ScannerParam ExtractInputParam(duckdb_data_chunk chunk, idx_t col_idx) {
	idx_t col_count = duckdb_data_chunk_get_column_count(chunk);
	if (col_idx >= col_count) {
		throw ScannerException("Cannot extract input parameter: column not found, column: " + std::to_string(col_idx) +
		                       ", columns count: " + std::to_string(col_count));
	}

	auto vec = duckdb_data_chunk_get_vector(chunk, col_idx);
	if (vec == nullptr) {
		throw ScannerException("Cannot extract input parameter: vector is NULL, column: " + std::to_string(col_idx) +
		                       ", columns count: " + std::to_string(col_count));
	}

	idx_t rows_count = duckdb_data_chunk_get_size(chunk);
	if (rows_count == 0) {
		throw ScannerException("Cannot extract input parameter: vector contains no rows, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	// DUCKDB_TYPE_SQLNULL
	uint64_t *validity = duckdb_vector_get_validity(vec);
	if (validity != nullptr && !duckdb_validity_row_is_valid(validity, 0)) {
		return ScannerParam();
	}

	auto ltype = LogicalTypePtr(duckdb_vector_get_column_type(vec), LogicalTypeDeleter);
	auto type_id = duckdb_get_type_id(ltype.get());
	switch (type_id) {
	case DUCKDB_TYPE_INTEGER:
		return ExtractIntegerNotNullInputParam(vec);
	case DUCKDB_TYPE_VARCHAR:
		return ExtractVarcharNotNullInputParam(vec);
	default: {
		throw ScannerException("Unsupported parameter type: " + std::to_string(type_id));
	}
	}
}

void SetOdbcParam(const std::string &query, HSTMT hstmt, ScannerParam &param, SQLSMALLINT param_idx) {
	switch (param.type_id) {
	case DUCKDB_TYPE_SQLNULL: {
		SetNullParam(query, hstmt, param_idx);
		return;
	}
	case DUCKDB_TYPE_INTEGER: {
		SetIntegerParam(query, hstmt, param, param_idx);
		return;
	}
	case DUCKDB_TYPE_VARCHAR: {
		SetVarcharParam(query, hstmt, param, param_idx);
		return;
	}
	default: {
		throw ScannerException("Unsupported parameter type: " + std::to_string(param.type_id));
	}
	}
}

} // namespace odbcscanner