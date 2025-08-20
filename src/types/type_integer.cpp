#include "types/type_integer.hpp"

#include "capi_pointers.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

std::pair<int32_t, bool> ExtractIntegerFunctionArg(duckdb_data_chunk chunk, idx_t col_idx) {
	idx_t col_count = duckdb_data_chunk_get_column_count(chunk);
	if (col_idx >= col_count) {
		throw ScannerException("Cannot extract INTEGER function argument: column not found, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	duckdb_vector vec = duckdb_data_chunk_get_vector(chunk, col_idx);
	if (vec == nullptr) {
		throw ScannerException("Cannot extract INTEGER function argument: vector is NULL, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	idx_t rows_count = duckdb_data_chunk_get_size(chunk);
	if (rows_count == 0) {
		throw ScannerException("Cannot extract INTEGER function argument: vector contains no rows, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	uint64_t *validity = duckdb_vector_get_validity(vec);
	if (validity != nullptr && !duckdb_validity_row_is_valid(validity, 0)) {
		return std::make_pair(-1, true);
	}

	int32_t *int64_data = reinterpret_cast<int32_t *>(duckdb_vector_get_data(vec));
	int32_t res = int64_data[0];
	return std::make_pair(res, false);
}

void AddIntegerResultColumn(duckdb_bind_info info, const std::string &name) {
	auto ltype = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_INTEGER), LogicalTypeDeleter);
	duckdb_bind_add_result_column(info, name.c_str(), ltype.get());
}

ScannerParam ExtractIntegerNotNullParam(duckdb_vector vec) {
	int32_t *data = reinterpret_cast<int32_t *>(duckdb_vector_get_data(vec));
	int32_t num = data[0];
	return ScannerParam(num);
}

void SetIntegerParam(const std::string &query, HSTMT hstmt, ScannerParam &param, SQLSMALLINT param_idx) {
	SQLRETURN ret = SQLBindParameter(hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0,
	                                 reinterpret_cast<SQLPOINTER>(&param.int32), param.len_bytes, &param.len_bytes);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = ReadDiagnostics(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' INTEGER failed, value: " + std::to_string(param.int32) +
		                       ", index: " + std::to_string(param_idx) + ", query: '" + query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

std::pair<int32_t, bool> FetchInteger(const std::string &query, HSTMT hstmt, SQLSMALLINT col_idx) {
	int32_t fetched = 0;
	SQLLEN ind;
	SQLRETURN ret = SQLGetData(hstmt, col_idx, SQL_C_SLONG, &fetched, sizeof(fetched), &ind);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = ReadDiagnostics(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' for INTEGER failed, column index: " + std::to_string(col_idx) +
		                       ", query: '" + query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag +
		                       "'");
	}
	return std::make_pair(fetched, ind == SQL_NULL_DATA);
}

void SetIntegerResult(duckdb_vector vec, idx_t row_idx, int32_t value) {
	int32_t *data = reinterpret_cast<int32_t *>(duckdb_vector_get_data(vec));
	data[row_idx] = value;
}

} // namespace odbcscanner
