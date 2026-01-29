#include "types.hpp"

#include <cstdint>

#include "capi_pointers.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

template <typename FLOAT_TYPE>
static ScannerValue ExtractNotNullParamInternal(duckdb_vector vec, idx_t row_idx) {
	FLOAT_TYPE *data = reinterpret_cast<FLOAT_TYPE *>(duckdb_vector_get_data(vec));
	FLOAT_TYPE num = data[row_idx];
	return ScannerValue(num);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<float>(DbmsQuirks &, duckdb_vector vec, idx_t row_idx) {
	return ExtractNotNullParamInternal<float>(vec, row_idx);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<double>(DbmsQuirks &, duckdb_vector vec, idx_t row_idx) {
	return ExtractNotNullParamInternal<double>(vec, row_idx);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<float>(DbmsQuirks &, duckdb_value value) {
	float val = duckdb_get_float(value);
	return ScannerValue(val);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<double>(DbmsQuirks &, duckdb_value value) {
	double val = duckdb_get_double(value);
	return ScannerValue(val);
}

template <typename FLOAT_TYPE>
static void BindOdbcParamInternal(QueryContext &ctx, SQLSMALLINT ctype, SQLSMALLINT sqltype, ScannerValue &param,
                                  SQLSMALLINT param_idx) {
	SQLRETURN ret = SQLBindParameter(ctx.hstmt(), param_idx, SQL_PARAM_INPUT, ctype, sqltype, 0, 0,
	                                 reinterpret_cast<SQLPOINTER>(&param.Value<FLOAT_TYPE>()), param.LengthBytes(),
	                                 &param.LengthBytes());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' failed, type: " + std::to_string(sqltype) +
		                       ", value: " + std::to_string(param.Value<FLOAT_TYPE>()) +
		                       ", index: " + std::to_string(param_idx) + ", query: '" + ctx.query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

template <>
void TypeSpecific::BindOdbcParam<float>(QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = param.ExpectedType() != SQL_PARAM_TYPE_UNKNOWN ? param.ExpectedType() : SQL_FLOAT;
	BindOdbcParamInternal<float>(ctx, SQL_C_FLOAT, sqltype, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<double>(QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = param.ExpectedType() != SQL_PARAM_TYPE_UNKNOWN ? param.ExpectedType() : SQL_DOUBLE;
	BindOdbcParamInternal<double>(ctx, SQL_C_DOUBLE, sqltype, param, param_idx);
}

template <typename FLOAT_TYPE>
void BindColumnInternal(QueryContext &ctx, SQLSMALLINT ctype, OdbcType &odbc_type, SQLSMALLINT col_idx) {
	if (!ctx.quirks.enable_columns_binding) {
		return;
	}
	ScannerValue nval(static_cast<FLOAT_TYPE>(0));
	ColumnBind nbind(std::move(nval));

	ColumnBind &bind = ctx.BindForColumn(col_idx);
	bind = std::move(nbind);
	FLOAT_TYPE &fetched = bind.Value<FLOAT_TYPE>();
	SQLLEN &ind = bind.Indicator();
	SQLRETURN ret = SQLBindCol(ctx.hstmt(), col_idx, ctype, &fetched, sizeof(fetched), &ind);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindCol' failed, C type: " + std::to_string(ctype) + ", column index: " +
		                       std::to_string(col_idx) + ", column type: " + odbc_type.ToString() + ",  query: '" +
		                       ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

template <>
void TypeSpecific::BindColumn<float>(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx) {
	BindColumnInternal<float>(ctx, SQL_C_FLOAT, odbc_type, col_idx);
}

template <>
void TypeSpecific::BindColumn<double>(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx) {
	BindColumnInternal<double>(ctx, SQL_C_DOUBLE, odbc_type, col_idx);
}

template <typename FLOAT_TYPE>
static void FetchAndSetResultInternal(QueryContext &ctx, SQLSMALLINT ctype, OdbcType &odbc_type, SQLSMALLINT col_idx,
                                      duckdb_vector vec, idx_t row_idx) {
	FLOAT_TYPE fetched_data = 0;
	FLOAT_TYPE *fetched_ptr = &fetched_data;
	SQLLEN ind = 0;

	if (ctx.quirks.enable_columns_binding) {
		ColumnBind &bind = ctx.BindForColumn(col_idx);
		FLOAT_TYPE &bound = bind.Value<FLOAT_TYPE>();
		fetched_ptr = &bound;
		ind = bind.ind;
	} else {
		SQLRETURN ret = SQLGetData(ctx.hstmt(), col_idx, ctype, &fetched_data, sizeof(fetched_data), &ind);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
			throw ScannerException("'SQLGetData' failed, C type: " + std::to_string(ctype) + ", column index: " +
			                       std::to_string(col_idx) + ", column type: " + odbc_type.ToString() + ",  query: '" +
			                       ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
		}
	}

	FLOAT_TYPE &fetched = *fetched_ptr;

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
