#include "params.hpp"

#include "common.hpp"
#include "scanner_exception.hpp"
#include "widechar.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

static void SetNull(const std::string &query, HSTMT hstmt, SQLSMALLINT param_idx) {
	SQLLEN ind = SQL_NULL_DATA;
	SQLRETURN ret = SQLBindParameter(hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 0, 0, NULL, 0, &ind);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = ReadDiagnostics(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' NULL failed, index: " + std::to_string(param_idx) + ", query: '" +
		                       query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

static void SetInteger(const std::string &query, HSTMT hstmt, duckdb_value value, SQLSMALLINT param_idx,
                       std::vector<void *> &holder) {
	int32_t num = duckdb_get_int32(value);
	void *num_ptr = std::malloc(sizeof(num));
	holder.push_back(num_ptr);
	std::memcpy(num_ptr, &num, sizeof(num));

	SQLLEN num_len = sizeof(num);
	void *num_len_ptr = std::malloc(sizeof(num_len));
	holder.push_back(num_len_ptr);
	std::memcpy(num_len_ptr, &num_len, sizeof(num_len));
	SQLRETURN ret =
	    SQLBindParameter(hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0,
	                     reinterpret_cast<SQLPOINTER>(num_ptr), num_len, reinterpret_cast<SQLLEN *>(num_len_ptr));
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = ReadDiagnostics(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' INTEGER failed, value: " + std::to_string(num) +
		                       ", index: " + std::to_string(param_idx) + ", query: '" + query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

static void SetVarchar(const std::string &query, HSTMT hstmt, duckdb_value value, SQLSMALLINT param_idx,
                       std::vector<void *> &holder) {
	auto str = VarcharPtr(duckdb_get_varchar(value), VarcharDeleter);
	size_t str_len = std::strlen(str.get());
	auto wstr = utf8_to_utf16_lenient(str.get(), str_len);
	size_t len_bytes_with_nt = (wstr.length<size_t>() + 1) * sizeof(SQLWCHAR);
	void *wstr_ptr = std::malloc(len_bytes_with_nt);
	holder.push_back(wstr_ptr);
	std::memcpy(wstr_ptr, reinterpret_cast<char *>(wstr.data()), len_bytes_with_nt);

	SQLLEN len_bytes = wstr.length<SQLLEN>() * sizeof(SQLWCHAR);
	void *len_bytes_ptr = std::malloc(sizeof(len_bytes));
	holder.push_back(len_bytes_ptr);
	std::memcpy(len_bytes_ptr, &len_bytes, sizeof(len_bytes));
	SQLRETURN ret =
	    SQLBindParameter(hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR, len_bytes, 0,
	                     reinterpret_cast<SQLPOINTER>(wstr_ptr), len_bytes, reinterpret_cast<SQLLEN *>(len_bytes_ptr));
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = ReadDiagnostics(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' VARCHAR failed, value: '" + std::string(str.get()) +
		                       "', index: " + std::to_string(param_idx) + ", query: '" + query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

void SetOdbcParam(const std::string &query, HSTMT hstmt, duckdb_value value, SQLSMALLINT param_idx,
                  std::vector<void *> &holder) {
	duckdb_logical_type val_type = duckdb_get_value_type(value);
	duckdb_type type_id = duckdb_get_type_id(val_type);

	switch (type_id) {
	case DUCKDB_TYPE_SQLNULL: {
		SetNull(query, hstmt, param_idx);
		return;
	}
	case DUCKDB_TYPE_INTEGER: {
		SetInteger(query, hstmt, value, param_idx, holder);
		return;
	}
	case DUCKDB_TYPE_VARCHAR: {
		SetVarchar(query, hstmt, value, param_idx, holder);
		return;
	}
	default: {
		throw ScannerException("Unsupported parameter type: " + std::to_string(type_id));
	}
	}
}

ValuePtr ExtractInputParam(duckdb_data_chunk input, idx_t col_idx) {
	std::string err_prefix = "Cannot extract input parameter, column: " + std::to_string(col_idx);

	auto vec = duckdb_data_chunk_get_vector(input, col_idx);
	if (vec == nullptr) {
		throw ScannerException(err_prefix + "vector is NULL");
	}

	// DUCKDB_TYPE_SQLNULL
	uint64_t *validity = duckdb_vector_get_validity(vec);
	if (validity != nullptr && !duckdb_validity_row_is_valid(validity, 0)) {
		return ValuePtr(duckdb_create_null_value(), ValueDeleter);
	}

	auto ltype = LogicalTypePtr(duckdb_vector_get_column_type(vec), LogicalTypeDeleter);
	auto type_id = duckdb_get_type_id(ltype.get());
	switch (type_id) {
	case DUCKDB_TYPE_INTEGER: {
		int32_t *data = reinterpret_cast<int32_t *>(duckdb_vector_get_data(vec));
		int32_t num = data[0];
		return ValuePtr(duckdb_create_int32(num), ValueDeleter);
	}
	case DUCKDB_TYPE_VARCHAR: {
		duckdb_string_t *data = reinterpret_cast<duckdb_string_t *>(duckdb_vector_get_data(vec));
		duckdb_string_t dstr = data[0];
		const char *cstr = duckdb_string_t_data(&dstr);
		uint32_t len = duckdb_string_t_length(dstr);
		return ValuePtr(duckdb_create_varchar_length(cstr, len), ValueDeleter);
	}
	default: {
		throw ScannerException("Unsupported parameter type: " + std::to_string(type_id));
	}
	}
}

} // namespace odbcscanner