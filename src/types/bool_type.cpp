#include "types.hpp"

#include <cstdint>

#include "capi_pointers.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<bool>(DbmsQuirks &, duckdb_vector vec) {
	bool *data = reinterpret_cast<bool *>(duckdb_vector_get_data(vec));
	bool val = data[0];
	return ScannerParam(val);
}

template <>
void TypeSpecific::BindOdbcParam<bool>(QueryContext &ctx, ScannerParam &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = param.ExpectedType() != SQL_PARAM_TYPE_UNKNOWN ? param.ExpectedType() : SQL_BIT;
	SQLRETURN ret =
	    SQLBindParameter(ctx.hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_BIT, sqltype, 0, 0,
	                     reinterpret_cast<SQLPOINTER>(&param.Value<bool>()), param.LengthBytes(), &param.LengthBytes());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' failed, type: " + std::to_string(sqltype) +
		                       ", value: " + std::to_string(param.Value<bool>()) +
		                       ", index: " + std::to_string(param_idx) + ", query: '" + ctx.query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

template <>
void TypeSpecific::FetchAndSetResult<bool>(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx,
                                           duckdb_vector vec, idx_t row_idx) {
	SQLCHAR fetched = 0;
	SQLLEN ind;
	SQLRETURN ret = SQLGetData(ctx.hstmt, col_idx, SQL_C_BIT, &fetched, sizeof(fetched), &ind);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' for failed, C type: " + std::to_string(SQL_C_BIT) + ", column index: " +
		                       std::to_string(col_idx) + ", column type: " + odbc_type.ToString() + ",  query: '" +
		                       ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}

	if (ind == SQL_NULL_DATA) {
		Types::SetNullValueToResult(vec, row_idx);
		return;
	}

	bool *data = reinterpret_cast<bool *>(duckdb_vector_get_data(vec));
	data[row_idx] = fetched == 1;
}

template <>
duckdb_type TypeSpecific::ResolveColumnType<bool>(QueryContext &, ResultColumn &) {
	return DUCKDB_TYPE_BOOLEAN;
}

} // namespace odbcscanner
