#include "types.hpp"

#include <cstdint>

#include "function_args.hpp"
#include "columns.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"

namespace odbcscanner {

template <typename INT_TYPE>
static std::pair<INT_TYPE, bool> ExtractFunctionArgInternal(LogicalTypeId expected_type, const std::string &type_name,
                                                            ScalarFunction::ExecInput &input, idx_t col_idx) {
	idx_t col_count = input.GetArgCount();
	if (col_idx >= col_count) {
		throw ScannerException("Cannot extract " + type_name + " function argument: column not found, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	Vector vec = input.GetArg(col_idx);
	if (!vec) {
		throw ScannerException("Cannot extract " + type_name + " function argument: vector is NULL, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	idx_t rows_count = input.GetRowCount();
	if (rows_count == 0) {
		throw ScannerException("Cannot extract " + type_name + " function argument: vector contains no rows, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	VectorView view = vec.GetView();
	idx_t row_idx = view.SelAt(0);
	if (!view.RowIsValid(row_idx)) {
		return std::make_pair(0, true);
	}

	auto vec_type = vec.GetLogicalType();
	if (!vec_type) {
		throw ScannerException("Cannot extract " + type_name + " function argument: column type is NULL, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	LogicalTypeId vec_type_id = Types::TypeIdOf(vec_type);
	if (vec_type_id != expected_type) {
		throw ScannerException("Cannot extract " + type_name + " function argument: invalid column type: " +
		                       std::to_string(static_cast<uint32_t>(vec_type_id)) +
		                       ", expected: " + std::to_string(static_cast<uint32_t>(expected_type)) + ", column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	const INT_TYPE *data = reinterpret_cast<const INT_TYPE *>(view.data);
	INT_TYPE res = data[row_idx];
	return std::make_pair(res, false);
}

template <>
std::pair<int32_t, bool> Types::ExtractFunctionArg<int32_t>(ScalarFunction::ExecInput &input, idx_t col_idx) {
	return ExtractFunctionArgInternal<int32_t>(LogicalTypeId::INTEGER, "INTEGER", input, col_idx);
}

template <>
std::pair<int64_t, bool> Types::ExtractFunctionArg<int64_t>(ScalarFunction::ExecInput &input, idx_t col_idx) {
	return ExtractFunctionArgInternal<int64_t>(LogicalTypeId::BIGINT, "BIGINT", input, col_idx);
}

template <typename INT_TYPE>
static ScannerValue ExtractNotNullParamInternal(Vector &vec, idx_t row_idx) {
	INT_TYPE *data = vec.GetDataMutable<INT_TYPE>();
	INT_TYPE num = data[row_idx];
	return ScannerValue(num);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<int8_t>(const DbmsQuirks &, Vector &vec, idx_t row_idx) {
	return ExtractNotNullParamInternal<int8_t>(vec, row_idx);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<uint8_t>(const DbmsQuirks &, Vector &vec, idx_t row_idx) {
	return ExtractNotNullParamInternal<uint8_t>(vec, row_idx);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<int16_t>(const DbmsQuirks &, Vector &vec, idx_t row_idx) {
	return ExtractNotNullParamInternal<int16_t>(vec, row_idx);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<uint16_t>(const DbmsQuirks &, Vector &vec, idx_t row_idx) {
	return ExtractNotNullParamInternal<uint16_t>(vec, row_idx);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<int32_t>(const DbmsQuirks &, Vector &vec, idx_t row_idx) {
	return ExtractNotNullParamInternal<int32_t>(vec, row_idx);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<uint32_t>(const DbmsQuirks &, Vector &vec, idx_t row_idx) {
	return ExtractNotNullParamInternal<uint32_t>(vec, row_idx);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<int64_t>(const DbmsQuirks &, Vector &vec, idx_t row_idx) {
	return ExtractNotNullParamInternal<int64_t>(vec, row_idx);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<uint64_t>(const DbmsQuirks &, Vector &vec, idx_t row_idx) {
	return ExtractNotNullParamInternal<uint64_t>(vec, row_idx);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<int8_t>(const DbmsQuirks &, const Value *value) {
	int8_t val = value->Get<int8_t>();
	return ScannerValue(val);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<uint8_t>(const DbmsQuirks &, const Value *value) {
	uint8_t val = value->Get<uint8_t>();
	return ScannerValue(val);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<int16_t>(const DbmsQuirks &, const Value *value) {
	int16_t val = value->Get<int16_t>();
	return ScannerValue(val);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<uint16_t>(const DbmsQuirks &, const Value *value) {
	uint16_t val = value->Get<uint16_t>();
	return ScannerValue(val);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<int32_t>(const DbmsQuirks &, const Value *value) {
	int32_t val = value->Get<int32_t>();
	return ScannerValue(val);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<uint32_t>(const DbmsQuirks &, const Value *value) {
	uint32_t val = value->Get<uint32_t>();
	return ScannerValue(val);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<int64_t>(const DbmsQuirks &, const Value *value) {
	int64_t val = value->Get<int64_t>();
	return ScannerValue(val);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<uint64_t>(const DbmsQuirks &, const Value *value) {
	uint64_t val = value->Get<uint64_t>();
	return ScannerValue(val);
}

template <typename INT_TYPE>
static void BindOdbcParamInternal(const QueryContext &ctx, SQLSMALLINT ctype, SQLSMALLINT sqltype, ScannerValue &param,
                                  SQLSMALLINT param_idx) {
	SQLRETURN ret = SQLBindParameter(ctx.hstmt(), param_idx, SQL_PARAM_INPUT, ctype, sqltype, 0, 0,
	                                 reinterpret_cast<SQLPOINTER>(&param.Value<INT_TYPE>()), param.LengthBytes(),
	                                 &param.LengthBytes());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' failed, type: " + std::to_string(sqltype) +
		                       ", value: " + std::to_string(param.Value<INT_TYPE>()) +
		                       ", index: " + std::to_string(param_idx) + ", query: '" + ctx.query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

static SQLSMALLINT IntegralSQLType(ScannerValue &param, SQLSMALLINT def_sqltype) {
	SQLSMALLINT expected = param.ExpectedType();
	switch (expected) {
	case SQL_TINYINT:
	case SQL_SMALLINT:
	case SQL_INTEGER:
	case SQL_BIGINT:
		return expected;
	}
	return def_sqltype;
}

// Note: if the prepared-parameter's expected SQL type is a character type
// (CHAR / VARCHAR / WCHAR family), the ScannerValue is expected to have been
// transformed to TYPE_DECIMAL_AS_CHARS by Params::SetExpectedTypes before we get
// here — dispatch will then route to BindOdbcParam<DecimalChars> instead of one of
// the integer specializations below. Stringifying in the scanner avoids the
// driver's numeric-C → character-SQL path, which has shipped silent-corruption bugs
// across several ODBC drivers (Firebird ≤ 3.5.0 in
// FirebirdSQL/firebird-odbc-driver#292; older MSSQL / MySQL releases too).

template <>
void TypeSpecific::BindOdbcParam<int8_t>(const QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = IntegralSQLType(param, SQL_TINYINT);
	BindOdbcParamInternal<int8_t>(ctx, SQL_C_STINYINT, sqltype, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<uint8_t>(const QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = IntegralSQLType(param, SQL_TINYINT);
	BindOdbcParamInternal<uint8_t>(ctx, SQL_C_UTINYINT, sqltype, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<int16_t>(const QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = IntegralSQLType(param, SQL_SMALLINT);
	BindOdbcParamInternal<int16_t>(ctx, SQL_C_SSHORT, sqltype, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<uint16_t>(const QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = IntegralSQLType(param, SQL_SMALLINT);
	BindOdbcParamInternal<uint16_t>(ctx, SQL_C_USHORT, sqltype, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<int32_t>(const QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = IntegralSQLType(param, SQL_INTEGER);
	BindOdbcParamInternal<int32_t>(ctx, SQL_C_SLONG, sqltype, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<uint32_t>(const QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = IntegralSQLType(param, SQL_INTEGER);
	BindOdbcParamInternal<uint32_t>(ctx, SQL_C_ULONG, sqltype, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<int64_t>(const QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = IntegralSQLType(param, SQL_BIGINT);
	BindOdbcParamInternal<int64_t>(ctx, SQL_C_SBIGINT, sqltype, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<uint64_t>(const QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = IntegralSQLType(param, SQL_BIGINT);
	BindOdbcParamInternal<uint64_t>(ctx, SQL_C_UBIGINT, sqltype, param, param_idx);
}

template <typename INT_TYPE>
void BindColumnInternal(const QueryContext &ctx, SQLSMALLINT ctype, const OdbcType &odbc_type, SQLSMALLINT col_idx) {
	if (!ctx.quirks.enable_columns_binding) {
		return;
	}
	ScannerValue nval(static_cast<INT_TYPE>(0));
	ColumnBind nbind(std::move(nval));

	ColumnBind &bind = ctx.BindForColumn(col_idx);
	bind = std::move(nbind);
	INT_TYPE &fetched = bind.Value<INT_TYPE>();
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
void TypeSpecific::BindColumn<int8_t>(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx) {
	BindColumnInternal<int8_t>(ctx, SQL_C_TINYINT, odbc_type, col_idx);
}

template <>
void TypeSpecific::BindColumn<uint8_t>(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx) {
	BindColumnInternal<uint8_t>(ctx, SQL_C_UTINYINT, odbc_type, col_idx);
}

template <>
void TypeSpecific::BindColumn<int16_t>(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx) {
	BindColumnInternal<int16_t>(ctx, SQL_C_SSHORT, odbc_type, col_idx);
}

template <>
void TypeSpecific::BindColumn<uint16_t>(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx) {
	BindColumnInternal<uint16_t>(ctx, SQL_C_USHORT, odbc_type, col_idx);
}

template <>
void TypeSpecific::BindColumn<int32_t>(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx) {
	BindColumnInternal<int32_t>(ctx, SQL_C_SLONG, odbc_type, col_idx);
}

template <>
void TypeSpecific::BindColumn<uint32_t>(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx) {
	BindColumnInternal<uint32_t>(ctx, SQL_C_ULONG, odbc_type, col_idx);
}

template <>
void TypeSpecific::BindColumn<int64_t>(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx) {
	BindColumnInternal<int64_t>(ctx, SQL_C_SBIGINT, odbc_type, col_idx);
}

template <>
void TypeSpecific::BindColumn<uint64_t>(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx) {
	BindColumnInternal<uint64_t>(ctx, SQL_C_UBIGINT, odbc_type, col_idx);
}

template <typename INT_TYPE>
static void FetchAndSetResultInternal(const QueryContext &ctx, SQLSMALLINT ctype, const OdbcType &odbc_type,
                                      SQLSMALLINT col_idx, Vector &vec, idx_t row_idx) {
	INT_TYPE fetched_data = 0;
	INT_TYPE *fetched_ptr = &fetched_data;
	SQLLEN ind = 0;

	if (ctx.quirks.enable_columns_binding) {
		ColumnBind &bind = ctx.BindForColumn(col_idx);
		INT_TYPE &bound = bind.Value<INT_TYPE>();
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

	INT_TYPE &fetched = *fetched_ptr;

	if (ind == SQL_NULL_DATA) {
		Types::SetNullValueToResult(vec, row_idx);
		return;
	}

	INT_TYPE *data = vec.GetDataMutable<INT_TYPE>();
	data[row_idx] = fetched;
}

template <>
void TypeSpecific::FetchAndSetResult<int8_t>(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx,
                                             Vector &vec, idx_t row_idx) {
	FetchAndSetResultInternal<int8_t>(ctx, SQL_C_STINYINT, odbc_type, col_idx, vec, row_idx);
}

template <>
void TypeSpecific::FetchAndSetResult<uint8_t>(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx,
                                              Vector &vec, idx_t row_idx) {
	FetchAndSetResultInternal<uint8_t>(ctx, SQL_C_UTINYINT, odbc_type, col_idx, vec, row_idx);
}

template <>
void TypeSpecific::FetchAndSetResult<int16_t>(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx,
                                              Vector &vec, idx_t row_idx) {
	FetchAndSetResultInternal<int16_t>(ctx, SQL_C_SSHORT, odbc_type, col_idx, vec, row_idx);
}

template <>
void TypeSpecific::FetchAndSetResult<uint16_t>(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx,
                                               Vector &vec, idx_t row_idx) {
	FetchAndSetResultInternal<uint16_t>(ctx, SQL_C_USHORT, odbc_type, col_idx, vec, row_idx);
}

template <>
void TypeSpecific::FetchAndSetResult<int32_t>(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx,
                                              Vector &vec, idx_t row_idx) {
	FetchAndSetResultInternal<int32_t>(ctx, SQL_C_SLONG, odbc_type, col_idx, vec, row_idx);
}

template <>
void TypeSpecific::FetchAndSetResult<uint32_t>(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx,
                                               Vector &vec, idx_t row_idx) {
	FetchAndSetResultInternal<uint32_t>(ctx, SQL_C_ULONG, odbc_type, col_idx, vec, row_idx);
}

template <>
void TypeSpecific::FetchAndSetResult<int64_t>(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx,
                                              Vector &vec, idx_t row_idx) {
	FetchAndSetResultInternal<int64_t>(ctx, SQL_C_SBIGINT, odbc_type, col_idx, vec, row_idx);
}

template <>
void TypeSpecific::FetchAndSetResult<uint64_t>(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx,
                                               Vector &vec, idx_t row_idx) {
	FetchAndSetResultInternal<uint64_t>(ctx, SQL_C_UBIGINT, odbc_type, col_idx, vec, row_idx);
}

template <>
LogicalTypeId TypeSpecific::ResolveColumnType<int8_t>(const QueryContext &, ResultColumn &) {
	return LogicalTypeId::TINYINT;
}

template <>
LogicalTypeId TypeSpecific::ResolveColumnType<uint8_t>(const QueryContext &, ResultColumn &) {
	return LogicalTypeId::UTINYINT;
}

template <>
LogicalTypeId TypeSpecific::ResolveColumnType<int16_t>(const QueryContext &, ResultColumn &) {
	return LogicalTypeId::SMALLINT;
}

template <>
LogicalTypeId TypeSpecific::ResolveColumnType<uint16_t>(const QueryContext &, ResultColumn &) {
	return LogicalTypeId::USMALLINT;
}

template <>
LogicalTypeId TypeSpecific::ResolveColumnType<int32_t>(const QueryContext &, ResultColumn &) {
	return LogicalTypeId::INTEGER;
}

template <>
LogicalTypeId TypeSpecific::ResolveColumnType<uint32_t>(const QueryContext &, ResultColumn &) {
	return LogicalTypeId::UINTEGER;
}

template <>
LogicalTypeId TypeSpecific::ResolveColumnType<int64_t>(const QueryContext &, ResultColumn &) {
	return LogicalTypeId::BIGINT;
}

template <>
LogicalTypeId TypeSpecific::ResolveColumnType<uint64_t>(const QueryContext &, ResultColumn &) {
	return LogicalTypeId::UBIGINT;
}

} // namespace odbcscanner
