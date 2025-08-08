#include "params.hpp"

#include "common.hpp"
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

ScannerParam ExtractInputParam(duckdb_data_chunk input, idx_t col_idx) {
	std::string err_prefix = "Cannot extract input parameter, column: " + std::to_string(col_idx);

	auto vec = duckdb_data_chunk_get_vector(input, col_idx);
	if (vec == nullptr) {
		throw ScannerException(err_prefix + "vector is NULL");
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
		return ExtractIntegerInputParam(vec);
	case DUCKDB_TYPE_VARCHAR:
		return ExtractVarcharInputParam(vec);
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