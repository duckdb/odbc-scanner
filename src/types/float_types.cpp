#include "types.hpp"

#include <cstdint>

#include "capi_pointers.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

template <typename FLOAT_TYPE>
static ScannerParam ExtractNotNullParamInternal(duckdb_vector vec) {
	FLOAT_TYPE *data = reinterpret_cast<FLOAT_TYPE *>(duckdb_vector_get_data(vec));
	FLOAT_TYPE num = data[0];
	return ScannerParam(num);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<float>(duckdb_vector vec) {
	return ExtractNotNullParamInternal<float>(vec);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<double>(duckdb_vector vec) {
	return ExtractNotNullParamInternal<double>(vec);
}

template <typename FLOAT_TYPE>
static void BindOdbcParamInternal(SQLSMALLINT ctype, SQLSMALLINT sqltype, const std::string &query, HSTMT hstmt,
                                  ScannerParam &param, SQLSMALLINT param_idx) {
	SQLRETURN ret = SQLBindParameter(hstmt, param_idx, SQL_PARAM_INPUT, ctype, sqltype, 0, 0,
	                                 reinterpret_cast<SQLPOINTER>(&param.Value<FLOAT_TYPE>()), param.LengthBytes(),
	                                 &param.LengthBytes());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' failed, type: " + std::to_string(sqltype) +
		                       ", value: " + std::to_string(param.Value<FLOAT_TYPE>()) +
		                       ", index: " + std::to_string(param_idx) + ", query: '" + query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

template <>
void TypeSpecific::BindOdbcParam<float>(const std::string &query, HSTMT hstmt, ScannerParam &param,
                                        SQLSMALLINT param_idx) {
	BindOdbcParamInternal<float>(SQL_C_FLOAT, SQL_FLOAT, query, hstmt, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<double>(const std::string &query, HSTMT hstmt, ScannerParam &param,
                                         SQLSMALLINT param_idx) {
	BindOdbcParamInternal<double>(SQL_C_DOUBLE, SQL_DOUBLE, query, hstmt, param, param_idx);
}

template <typename FLOAT_TYPE>
static void FetchAndSetResultInternal(SQLSMALLINT ctype, OdbcType &odbc_type, const std::string &query, HSTMT hstmt,
                                      SQLSMALLINT col_idx, duckdb_vector vec, idx_t row_idx) {
	FLOAT_TYPE fetched = 0;
	SQLLEN ind;
	SQLRETURN ret = SQLGetData(hstmt, col_idx, ctype, &fetched, sizeof(fetched), &ind);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' for failed, C type: " + std::to_string(ctype) + ", column index: " +
		                       std::to_string(col_idx) + ", column type: " + odbc_type.ToString() + ",  query: '" +
		                       query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}

	if (ind == SQL_NULL_DATA) {
		Types::SetNullValueToResult(vec, row_idx);
		return;
	}

	FLOAT_TYPE *data = reinterpret_cast<FLOAT_TYPE *>(duckdb_vector_get_data(vec));
	data[row_idx] = fetched;
}

template <>
void TypeSpecific::FetchAndSetResult<float>(OdbcType &odbc_type, const std::string &query, HSTMT hstmt,
                                            SQLSMALLINT col_idx, duckdb_vector vec, idx_t row_idx) {
	FetchAndSetResultInternal<float>(SQL_C_FLOAT, odbc_type, query, hstmt, col_idx, vec, row_idx);
}

template <>
void TypeSpecific::FetchAndSetResult<double>(OdbcType &odbc_type, const std::string &query, HSTMT hstmt,
                                             SQLSMALLINT col_idx, duckdb_vector vec, idx_t row_idx) {
	FetchAndSetResultInternal<double>(SQL_C_DOUBLE, odbc_type, query, hstmt, col_idx, vec, row_idx);
}

} // namespace odbcscanner
