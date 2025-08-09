#include "types/type_integer.hpp"

#include "capi_pointers.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

void AddIntegerResultColumn(duckdb_bind_info info, const std::string &name) {
	auto ltype = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_INTEGER), LogicalTypeDeleter);
	duckdb_bind_add_result_column(info, name.c_str(), ltype.get());
}

ScannerParam ExtractIntegerNotNullInputParam(duckdb_vector vec) {
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
