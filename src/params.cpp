#include "params.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include "diagnostics.hpp"
#include "scanner_exception.hpp"
#include "types/type_integer.hpp"
#include "types/type_null.hpp"
#include "types/type_varchar.hpp"
#include "widechar.hpp"

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

std::vector<ScannerParam> ExtractStructParamsFromChunk(duckdb_data_chunk chunk, idx_t col_idx) {
	(void)col_idx;
	idx_t col_count = duckdb_data_chunk_get_column_count(chunk);
	if (col_idx >= col_count) {
		throw ScannerException("Cannot extract parameters from STRUCT: column not found, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	duckdb_vector vec = duckdb_data_chunk_get_vector(chunk, col_idx);
	if (vec == nullptr) {
		throw ScannerException("Cannot extract parameters from STRUCT: vector is NULL, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	idx_t rows_count = duckdb_data_chunk_get_size(chunk);
	if (rows_count == 0) {
		throw ScannerException("Cannot extract parameters from STRUCT: vector contains no rows, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	uint64_t *validity = duckdb_vector_get_validity(vec);
	if (validity != nullptr && !duckdb_validity_row_is_valid(validity, 0)) {
		throw ScannerException("Cannot extract parameters from STRUCT: specified STRUCT is NULL, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	auto struct_type = LogicalTypePtr(duckdb_vector_get_column_type(vec), LogicalTypeDeleter);
	duckdb_type struct_type_id = duckdb_get_type_id(struct_type.get());
	if (struct_type_id != DUCKDB_TYPE_STRUCT) {
		throw ScannerException(
		    "Cannot extract parameters from STRUCT: specified value is not STRUCT, column: " + std::to_string(col_idx) +
		    ", columns count: " + std::to_string(col_count) + ", type ID: " + std::to_string(struct_type_id));
	}

	idx_t params_count = duckdb_struct_type_child_count(struct_type.get());
	if (params_count == 0) {
		throw ScannerException("Cannot extract parameters from STRUCT: specified STRUCT has no fields, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	std::vector<ScannerParam> params;
	for (idx_t i = 0; i < params_count; i++) {
		auto child_vec = duckdb_struct_vector_get_child(vec, i);
		if (child_vec == nullptr) {
			throw ScannerException(
			    "Cannot extract parameters from STRUCT: child vector is NULL, index: " + std::to_string(i) +
			    ", column: " + std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
		}

		uint64_t *validity = duckdb_vector_get_validity(child_vec);
		if (validity != nullptr && !duckdb_validity_row_is_valid(validity, 0)) {
			params.emplace_back(ScannerParam());
			continue;
		}

		auto child_type = LogicalTypePtr(duckdb_struct_type_child_type(struct_type.get(), i), LogicalTypeDeleter);
		auto child_type_id = duckdb_get_type_id(child_type.get());
		switch (child_type_id) {
		case DUCKDB_TYPE_INTEGER: {
			ScannerParam sp = ExtractIntegerNotNullParam(child_vec);
			params.emplace_back(std::move(sp));
			break;
		}
		case DUCKDB_TYPE_VARCHAR: {
			ScannerParam sp = ExtractVarcharNotNullParam(child_vec);
			params.emplace_back(std::move(sp));
			break;
		}
		default:
			throw ScannerException(
			    "Cannot extract parameters from STRUCT: specified specified type is not supported, id: " +
			    std::to_string(child_type_id) + ", index: " + std::to_string(i) +
			    ", column: " + std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
		}
	}

	return params;
}

std::vector<ScannerParam> ExtractStructParamsFromValue(duckdb_value struct_value) {
	if (duckdb_is_null_value(struct_value)) {
		throw ScannerException("Cannot extract parameters from STRUCT: specified STRUCT is NULL");
	}
	duckdb_logical_type struct_type = duckdb_get_value_type(struct_value);
	duckdb_type struct_type_id = duckdb_get_type_id(struct_type);
	if (struct_type_id != DUCKDB_TYPE_STRUCT) {
		throw ScannerException("Cannot extract parameters from STRUCT: specified value is not STRUCT");
	}
	idx_t params_count = duckdb_struct_type_child_count(struct_type);

	std::vector<ScannerParam> params;
	for (idx_t i = 0; i < params_count; i++) {
		auto child_val = ValuePtr(duckdb_get_struct_child(struct_value, i), ValueDeleter);
		if (duckdb_is_null_value(child_val.get())) {
			params.emplace_back(ScannerParam());
			continue;
		}

		auto child_type = LogicalTypePtr(duckdb_struct_type_child_type(struct_type, i), LogicalTypeDeleter);
		auto child_type_id = duckdb_get_type_id(child_type.get());
		switch (child_type_id) {
		case DUCKDB_TYPE_INTEGER: {
			int32_t num = duckdb_get_int32(child_val.get());
			params.emplace_back(ScannerParam(num));
			break;
		}
		case DUCKDB_TYPE_VARCHAR: {
			char *cstr = duckdb_get_varchar(child_val.get());
			std::string str(cstr);
			params.emplace_back(ScannerParam(str));
			break;
		}
		default:
			throw ScannerException(
			    "Cannot extract parameters from STRUCT: specified specified type is not supported, id: " +
			    std::to_string(child_type_id) + ", index: " + std::to_string(i));
		}
	}

	return params;
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

void ResetOdbcParams(HSTMT hstmt) {
	SQLRETURN ret = SQLFreeStmt(hstmt, SQL_RESET_PARAMS);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = ReadDiagnostics(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLFreeStmt' SQL_RESET_PARAMS failed, diagnostics: '" + diag + "'");
	}
}

} // namespace odbcscanner
