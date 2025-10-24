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
ScannerParam TypeSpecific::ExtractNotNullParam<float>(DbmsQuirks &, duckdb_vector vec) {
	return ExtractNotNullParamInternal<float>(vec);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<double>(DbmsQuirks &, duckdb_vector vec) {
	return ExtractNotNullParamInternal<double>(vec);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<float>(DbmsQuirks &, duckdb_value value) {
	float val = duckdb_get_float(value);
	return ScannerParam(val);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<double>(DbmsQuirks &, duckdb_value value) {
	double val = duckdb_get_double(value);
	return ScannerParam(val);
}

template <typename FLOAT_TYPE>
static void BindOdbcParamInternal(QueryContext &ctx, SQLSMALLINT ctype, SQLSMALLINT sqltype, ScannerParam &param,
                                  SQLSMALLINT param_idx) {
	SQLRETURN ret = SQLBindParameter(ctx.hstmt, param_idx, SQL_PARAM_INPUT, ctype, sqltype, 0, 0,
	                                 reinterpret_cast<SQLPOINTER>(&param.Value<FLOAT_TYPE>()), param.LengthBytes(),
	                                 &param.LengthBytes());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' failed, type: " + std::to_string(sqltype) +
		                       ", value: " + std::to_string(param.Value<FLOAT_TYPE>()) +
		                       ", index: " + std::to_string(param_idx) + ", query: '" + ctx.query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

template <>
void TypeSpecific::BindOdbcParam<float>(QueryContext &ctx, ScannerParam &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = param.ExpectedType() != SQL_PARAM_TYPE_UNKNOWN ? param.ExpectedType() : SQL_FLOAT;
	BindOdbcParamInternal<float>(ctx, SQL_C_FLOAT, sqltype, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<double>(QueryContext &ctx, ScannerParam &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = param.ExpectedType() != SQL_PARAM_TYPE_UNKNOWN ? param.ExpectedType() : SQL_DOUBLE;
	BindOdbcParamInternal<double>(ctx, SQL_C_DOUBLE, sqltype, param, param_idx);
}

template <typename FLOAT_TYPE>
static void FetchAndSetResultInternal(QueryContext &ctx, SQLSMALLINT ctype, OdbcType &odbc_type, SQLSMALLINT col_idx,
                                      duckdb_vector vec, idx_t row_idx) {
	FLOAT_TYPE fetched = 0;
	SQLLEN ind;
	SQLRETURN ret = SQLGetData(ctx.hstmt, col_idx, ctype, &fetched, sizeof(fetched), &ind);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' for failed, C type: " + std::to_string(ctype) + ", column index: " +
		                       std::to_string(col_idx) + ", column type: " + odbc_type.ToString() + ",  query: '" +
		                       ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}

	if (ind == SQL_NULL_DATA) {
		Types::SetNullValueToResult(vec, row_idx);
		return;
	}

	FLOAT_TYPE *data = reinterpret_cast<FLOAT_TYPE *>(duckdb_vector_get_data(vec));
	data[row_idx] = fetched;
}

template <>
void TypeSpecific::FetchAndSetResult<float>(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx,
                                            duckdb_vector vec, idx_t row_idx) {
	FetchAndSetResultInternal<float>(ctx, SQL_C_FLOAT, odbc_type, col_idx, vec, row_idx);
}

template <>
void TypeSpecific::FetchAndSetResult<double>(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx,
                                             duckdb_vector vec, idx_t row_idx) {
	FetchAndSetResultInternal<double>(ctx, SQL_C_DOUBLE, odbc_type, col_idx, vec, row_idx);
}

template <>
duckdb_type TypeSpecific::ResolveColumnType<float>(QueryContext &, ResultColumn &) {
	return DUCKDB_TYPE_FLOAT;
}

template <>
duckdb_type TypeSpecific::ResolveColumnType<double>(QueryContext &, ResultColumn &) {
	return DUCKDB_TYPE_DOUBLE;
}

} // namespace odbcscanner
