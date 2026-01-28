#include "types.hpp"

#include <cstdint>

#include "capi_pointers.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<bool>(DbmsQuirks &, duckdb_vector vec, idx_t row_idx) {
	bool *data = reinterpret_cast<bool *>(duckdb_vector_get_data(vec));
	bool val = data[row_idx];
	return ScannerValue(val);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<bool>(DbmsQuirks &, duckdb_value value) {
	bool val = duckdb_get_bool(value);
	return ScannerValue(val);
}

template <>
void TypeSpecific::BindOdbcParam<bool>(QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = param.ExpectedType() != SQL_PARAM_TYPE_UNKNOWN ? param.ExpectedType() : SQL_BIT;
	bool &val = param.Value<bool>();
	SQLRETURN ret = SQLBindParameter(ctx.hstmt(), param_idx, SQL_PARAM_INPUT, SQL_C_BIT, sqltype, 0, 0,
	                                 reinterpret_cast<SQLPOINTER>(&val), param.LengthBytes(), &param.LengthBytes());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' failed, type: " + std::to_string(sqltype) +
		                       ", value: " + std::to_string(param.Value<bool>()) +
		                       ", index: " + std::to_string(param_idx) + ", query: '" + ctx.query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

template <>
void TypeSpecific::BindColumn<bool>(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx) {
	if (!ctx.quirks.enable_columns_binding) {
		return;
	}
	SqlBit sb;
	ScannerValue nval(sb);
	ColumnBind nbind(std::move(nval));

	ColumnBind &bind = ctx.BindForColumn(col_idx);
	bind = std::move(nbind);
	SqlBit &fetched = bind.Value<SqlBit>();
	SQLLEN &ind = bind.Indicator();
	SQLRETURN ret = SQLBindCol(ctx.hstmt(), col_idx, SQL_C_BIT, &fetched.val, sizeof(fetched.val), &ind);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindCol' failed, C type: " + std::to_string(SQL_C_BIT) + ", column index: " +
		                       std::to_string(col_idx) + ", column type: " + odbc_type.ToString() + ",  query: '" +
		                       ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

template <>
void TypeSpecific::FetchAndSetResult<bool>(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx,
                                           duckdb_vector vec, idx_t row_idx) {
	SqlBit fetched_data;
	SqlBit *fetched_ptr = &fetched_data;
	SQLLEN ind = 0;

	if (ctx.quirks.enable_columns_binding) {
		ColumnBind &bind = ctx.BindForColumn(col_idx);
		SqlBit &bound = bind.Value<SqlBit>();
		fetched_ptr = &bound;
		ind = bind.ind;
	} else {
		SQLRETURN ret = SQLGetData(ctx.hstmt(), col_idx, SQL_C_BIT, &fetched_data.val, sizeof(fetched_data.val), &ind);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
			throw ScannerException("'SQLGetData' failed, C type: " + std::to_string(SQL_C_BIT) + ", column index: " +
			                       std::to_string(col_idx) + ", column type: " + odbc_type.ToString() + ",  query: '" +
			                       ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
		}
	}

	SqlBit &fetched = *fetched_ptr;

	if (ind == SQL_NULL_DATA) {
		Types::SetNullValueToResult(vec, row_idx);
		return;
	}

	bool *data = reinterpret_cast<bool *>(duckdb_vector_get_data(vec));
	data[row_idx] = fetched.val == 1;
}

template <>
duckdb_type TypeSpecific::ResolveColumnType<bool>(QueryContext &, ResultColumn &) {
	return DUCKDB_TYPE_BOOLEAN;
}

} // namespace odbcscanner
