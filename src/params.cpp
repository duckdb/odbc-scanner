#include "params.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include "diagnostics.hpp"
#include "scanner_exception.hpp"
#include "types.hpp"
#include "widechar.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

ScannerParam::ScannerParam() : type_id(DUCKDB_TYPE_SQLNULL) {
}

ScannerParam::ScannerParam(int32_t value) : type_id(DUCKDB_TYPE_INTEGER), len_bytes(sizeof(value)), val(value) {
}

ScannerParam::ScannerParam(int64_t value) : type_id(DUCKDB_TYPE_BIGINT), len_bytes(sizeof(value)), val(value) {
}

ScannerParam::ScannerParam(const char *cstr, size_t len) : type_id(DUCKDB_TYPE_VARCHAR) {
	WideString wstr = WideChar::Widen(cstr, len);
	new (&this->val.wstr) WideString;
	this->val.wstr = std::move(wstr);
	this->len_bytes = val.wstr.length<SQLLEN>() * sizeof(SQLWCHAR);
}

ScannerParam::ScannerParam(ScannerParam &&other) : type_id(other.type_id), len_bytes(other.len_bytes) {
	switch (type_id) {
	case DUCKDB_TYPE_SQLNULL:
		break;
	case DUCKDB_TYPE_BIGINT:
		this->val.int64 = other.Int64();
		break;
	case DUCKDB_TYPE_INTEGER:
		this->val.int32 = other.Int32();
		break;
	case DUCKDB_TYPE_VARCHAR:
		new (&this->val.wstr) WideString;
		this->val.wstr = std::move(other.Utf16String());
		break;
	default:
		throw ScannerException("Unsupported parameter type, ID: " + std::to_string(type_id));
	}
}

ScannerParam &ScannerParam::operator=(ScannerParam &&other) {
	this->type_id = other.type_id;
	this->len_bytes = other.len_bytes;
	switch (type_id) {
	case DUCKDB_TYPE_SQLNULL:
		break;
	case DUCKDB_TYPE_BIGINT:
		this->val.int64 = other.Int64();
		break;
	case DUCKDB_TYPE_INTEGER:
		this->val.int32 = other.Int32();
		break;
	case DUCKDB_TYPE_VARCHAR:
		new (&this->val.wstr) WideString;
		this->val.wstr = std::move(other.Utf16String());
		break;
	default:
		throw ScannerException("Unsupported parameter type, ID: " + std::to_string(type_id));
	}
	return *this;
}

ScannerParam::~ScannerParam() {
	switch (type_id) {
	case DUCKDB_TYPE_VARCHAR:
		this->val.wstr.~WideString();
		break;
	default: {
		// no-op
	}
	}
}

std::string ScannerParam::ToUtf8String(size_t max_len) {
	WideString &wstr = Utf16String();
	size_t len = std::min(wstr.length<size_t>(), max_len);
	return WideChar::Narrow(wstr.data(), len);
}

duckdb_type ScannerParam::TypeId() {
	return type_id;
}

SQLLEN &ScannerParam::LengthBytes() {
	return len_bytes;
}

void ScannerParam::CheckType(duckdb_type expected) {
	if (type_id != expected) {
		throw ScannerException("Invalid parameter type, expected: " + std::to_string(expected) +
		                       ", actual: " + std::to_string(type_id));
	}
}

int32_t &ScannerParam::Int32() {
	CheckType(DUCKDB_TYPE_INTEGER);
	return val.int32;
}

int64_t &ScannerParam::Int64() {
	CheckType(DUCKDB_TYPE_BIGINT);
	return val.int64;
}

WideString &ScannerParam::Utf16String() {
	CheckType(DUCKDB_TYPE_VARCHAR);
	return val.wstr;
}

std::vector<ScannerParam> Params::Extract(duckdb_data_chunk chunk, idx_t col_idx) {
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
		ScannerParam sp = Types::ExtractNotNullParamOfType(child_type_id, child_vec, i);
		params.emplace_back(std::move(sp));
	}

	return params;
}

std::vector<ScannerParam> Params::Extract(duckdb_value struct_value) {
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
		ScannerParam sp = Types::ExtractNotNullParamOfType(child_type_id, child_val.get(), i);
		params.emplace_back(std::move(sp));
	}

	return params;
}

void Params::BindToOdbc(const std::string &query, HSTMT hstmt, std::vector<ScannerParam> &params) {
	if (params.size() == 0) {
		return;
	}

	SQLRETURN ret = SQLFreeStmt(hstmt, SQL_RESET_PARAMS);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLFreeStmt' SQL_RESET_PARAMS failed, diagnostics: '" + diag + "'");
	}

	for (size_t i = 0; i < params.size(); i++) {
		ScannerParam &param = params.at(i);
		SQLSMALLINT idx = static_cast<SQLSMALLINT>(i + 1);
		Types::BindOdbcParam(query, hstmt, param, idx);
	}
}

} // namespace odbcscanner
