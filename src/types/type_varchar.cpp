#include "types/type_varchar.hpp"

#include "capi_pointers.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

std::pair<std::string, bool> ExtractVarcharFunctionArg(duckdb_data_chunk chunk, idx_t col_idx) {
	idx_t col_count = duckdb_data_chunk_get_column_count(chunk);
	if (col_idx >= col_count) {
		throw ScannerException("Cannot extract VARCHAR function argument: column not found, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	duckdb_vector vec = duckdb_data_chunk_get_vector(chunk, col_idx);
	if (vec == nullptr) {
		throw ScannerException("Cannot extract VARCHAR function argument: vector is NULL, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	idx_t rows_count = duckdb_data_chunk_get_size(chunk);
	if (rows_count == 0) {
		throw ScannerException("Cannot extract VARCHAR function argument: vector contains no rows, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	uint64_t *validity = duckdb_vector_get_validity(vec);
	if (validity != nullptr && !duckdb_validity_row_is_valid(validity, 0)) {
		return std::make_pair("", true);
	}

	duckdb_string_t *str_data = reinterpret_cast<duckdb_string_t *>(duckdb_vector_get_data(vec));
	duckdb_string_t str_t = str_data[0];

	const char *cstr = duckdb_string_t_data(&str_t);
	uint32_t len = duckdb_string_t_length(str_t);
	std::string res(cstr, len);

	return std::make_pair(std::move(res), false);
}

void AddVarcharResultColumn(duckdb_bind_info info, const std::string &name) {
	auto ltype = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_VARCHAR), LogicalTypeDeleter);
	duckdb_bind_add_result_column(info, name.c_str(), ltype.get());
}

ScannerParam ExtractVarcharNotNullInputParam(duckdb_vector vec) {
	duckdb_string_t *data = reinterpret_cast<duckdb_string_t *>(duckdb_vector_get_data(vec));
	duckdb_string_t dstr = data[0];
	const char *cstr = duckdb_string_t_data(&dstr);
	uint32_t len = duckdb_string_t_length(dstr);
	return ScannerParam(std::string(cstr, len));
}

void SetVarcharParam(const std::string &query, HSTMT hstmt, ScannerParam &param, SQLSMALLINT param_idx) {
	SQLRETURN ret =
	    SQLBindParameter(hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR, param.len_bytes, 0,
	                     reinterpret_cast<SQLPOINTER>(param.wstr.data()), param.len_bytes, &param.len_bytes);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = ReadDiagnostics(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' VARCHAR failed, value: '" + param.str +
		                       "', index: " + std::to_string(param_idx) + ", query: '" + query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

std::pair<std::string, bool> FetchVarchar(const std::string &query, HSTMT hstmt, SQLSMALLINT col_idx) {
	// todo: parts
	std::vector<SQLWCHAR> buf;
	buf.resize(1024);
	SQLLEN len = 0;
	SQLRETURN ret = SQLGetData(hstmt, col_idx, SQL_C_WCHAR, buf.data(),
	                           static_cast<SQLSMALLINT>(buf.size() * sizeof(SQLWCHAR)), &len);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = ReadDiagnostics(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' for VARCHAR failed, column index: " + std::to_string(col_idx) +
		                       ", query: '" + query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag +
		                       "'");
	}
	if (len != SQL_NULL_DATA) {
		std::string str = utf16_to_utf8_lenient(buf.data(), len / sizeof(SQLWCHAR));
		return std::make_pair(std::move(str), false);
	} else {
		return std::make_pair("", true);
	}
}

void SetVarcharResult(duckdb_vector vec, idx_t row_idx, const std::string &value) {
	duckdb_vector_assign_string_element_len(vec, row_idx, value.c_str(), value.length());
}

} // namespace odbcscanner
