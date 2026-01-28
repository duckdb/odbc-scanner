#include "types.hpp"

#include <cstdint>

#include "capi_pointers.hpp"
#include "columns.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

template <typename INT_TYPE>
static std::pair<INT_TYPE, bool> ExtractFunctionArgInternal(const std::string &type_name, duckdb_data_chunk chunk,
                                                            idx_t col_idx) {
	idx_t col_count = duckdb_data_chunk_get_column_count(chunk);
	if (col_idx >= col_count) {
		throw ScannerException("Cannot extract " + type_name + " function argument: column not found, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	duckdb_vector vec = duckdb_data_chunk_get_vector(chunk, col_idx);
	if (vec == nullptr) {
		throw ScannerException("Cannot extract " + type_name + " function argument: vector is NULL, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	idx_t rows_count = duckdb_data_chunk_get_size(chunk);
	if (rows_count == 0) {
		throw ScannerException("Cannot extract " + type_name + " function argument: vector contains no rows, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	uint64_t *validity = duckdb_vector_get_validity(vec);
	if (validity != nullptr && !duckdb_validity_row_is_valid(validity, 0)) {
		return std::make_pair(0, true);
	}

	INT_TYPE *data = reinterpret_cast<INT_TYPE *>(duckdb_vector_get_data(vec));
	INT_TYPE res = data[0];
	return std::make_pair(res, false);
}

template <>
std::pair<int32_t, bool> Types::ExtractFunctionArg<int32_t>(duckdb_data_chunk chunk, idx_t col_idx) {
	return ExtractFunctionArgInternal<int32_t>("INTEGER", chunk, col_idx);
}

template <>
std::pair<int64_t, bool> Types::ExtractFunctionArg<int64_t>(duckdb_data_chunk chunk, idx_t col_idx) {
	return ExtractFunctionArgInternal<int64_t>("BIGINT", chunk, col_idx);
}

template <typename INT_TYPE>
static ScannerValue ExtractNotNullParamInternal(duckdb_vector vec, idx_t row_idx) {
	INT_TYPE *data = reinterpret_cast<INT_TYPE *>(duckdb_vector_get_data(vec));
	INT_TYPE num = data[row_idx];
	return ScannerValue(num);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<int8_t>(DbmsQuirks &, duckdb_vector vec, idx_t row_idx) {
	return ExtractNotNullParamInternal<int8_t>(vec, row_idx);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<uint8_t>(DbmsQuirks &, duckdb_vector vec, idx_t row_idx) {
	return ExtractNotNullParamInternal<uint8_t>(vec, row_idx);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<int16_t>(DbmsQuirks &, duckdb_vector vec, idx_t row_idx) {
	return ExtractNotNullParamInternal<int16_t>(vec, row_idx);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<uint16_t>(DbmsQuirks &, duckdb_vector vec, idx_t row_idx) {
	return ExtractNotNullParamInternal<uint16_t>(vec, row_idx);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<int32_t>(DbmsQuirks &, duckdb_vector vec, idx_t row_idx) {
	return ExtractNotNullParamInternal<int32_t>(vec, row_idx);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<uint32_t>(DbmsQuirks &, duckdb_vector vec, idx_t row_idx) {
	return ExtractNotNullParamInternal<uint32_t>(vec, row_idx);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<int64_t>(DbmsQuirks &, duckdb_vector vec, idx_t row_idx) {
	return ExtractNotNullParamInternal<int64_t>(vec, row_idx);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<uint64_t>(DbmsQuirks &, duckdb_vector vec, idx_t row_idx) {
	return ExtractNotNullParamInternal<uint64_t>(vec, row_idx);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<int8_t>(DbmsQuirks &, duckdb_value value) {
	int8_t val = duckdb_get_int8(value);
	return ScannerValue(val);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<uint8_t>(DbmsQuirks &, duckdb_value value) {
	uint8_t val = duckdb_get_uint8(value);
	return ScannerValue(val);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<int16_t>(DbmsQuirks &, duckdb_value value) {
	int16_t val = duckdb_get_int16(value);
	return ScannerValue(val);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<uint16_t>(DbmsQuirks &, duckdb_value value) {
	uint16_t val = duckdb_get_uint16(value);
	return ScannerValue(val);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<int32_t>(DbmsQuirks &, duckdb_value value) {
	int32_t val = duckdb_get_int32(value);
	return ScannerValue(val);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<uint32_t>(DbmsQuirks &, duckdb_value value) {
	uint32_t val = duckdb_get_uint32(value);
	return ScannerValue(val);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<int64_t>(DbmsQuirks &, duckdb_value value) {
	int64_t val = duckdb_get_int64(value);
	return ScannerValue(val);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<uint64_t>(DbmsQuirks &, duckdb_value value) {
	uint64_t val = duckdb_get_uint64(value);
	return ScannerValue(val);
}

template <typename INT_TYPE>
static void BindOdbcParamInternal(QueryContext &ctx, SQLSMALLINT ctype, SQLSMALLINT sqltype, ScannerValue &param,
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

template <>
void TypeSpecific::BindOdbcParam<int8_t>(QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = IntegralSQLType(param, SQL_TINYINT);
	BindOdbcParamInternal<int8_t>(ctx, SQL_C_STINYINT, sqltype, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<uint8_t>(QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = IntegralSQLType(param, SQL_TINYINT);
	BindOdbcParamInternal<uint8_t>(ctx, SQL_C_UTINYINT, sqltype, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<int16_t>(QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = IntegralSQLType(param, SQL_SMALLINT);
	BindOdbcParamInternal<int16_t>(ctx, SQL_C_SSHORT, sqltype, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<uint16_t>(QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = IntegralSQLType(param, SQL_SMALLINT);
	BindOdbcParamInternal<uint16_t>(ctx, SQL_C_USHORT, sqltype, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<int32_t>(QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = IntegralSQLType(param, SQL_INTEGER);
	BindOdbcParamInternal<int32_t>(ctx, SQL_C_SLONG, sqltype, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<uint32_t>(QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = IntegralSQLType(param, SQL_INTEGER);
	BindOdbcParamInternal<uint32_t>(ctx, SQL_C_ULONG, sqltype, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<int64_t>(QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = IntegralSQLType(param, SQL_BIGINT);
	BindOdbcParamInternal<int64_t>(ctx, SQL_C_SBIGINT, sqltype, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<uint64_t>(QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = IntegralSQLType(param, SQL_BIGINT);
	BindOdbcParamInternal<uint64_t>(ctx, SQL_C_UBIGINT, sqltype, param, param_idx);
}

template <typename INT_TYPE>
static void FetchAndSetResultInternal(QueryContext &ctx, SQLSMALLINT ctype, OdbcType &odbc_type, SQLSMALLINT col_idx,
                                      duckdb_vector vec, idx_t row_idx) {
	INT_TYPE fetched = 0;
	SQLLEN ind;
	SQLRETURN ret = SQLGetData(ctx.hstmt(), col_idx, ctype, &fetched, sizeof(fetched), &ind);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' failed, C type: " + std::to_string(ctype) + ", column index: " +
		                       std::to_string(col_idx) + ", column type: " + odbc_type.ToString() + ",  query: '" +
		                       ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}

	if (ind == SQL_NULL_DATA) {
		Types::SetNullValueToResult(vec, row_idx);
		return;
	}

	INT_TYPE *data = reinterpret_cast<INT_TYPE *>(duckdb_vector_get_data(vec));
	data[row_idx] = fetched;
}

template <>
void TypeSpecific::FetchAndSetResult<int8_t>(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx,
                                             duckdb_vector vec, idx_t row_idx) {
	FetchAndSetResultInternal<int8_t>(ctx, SQL_C_STINYINT, odbc_type, col_idx, vec, row_idx);
}

template <>
void TypeSpecific::FetchAndSetResult<uint8_t>(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx,
                                              duckdb_vector vec, idx_t row_idx) {
	FetchAndSetResultInternal<uint8_t>(ctx, SQL_C_UTINYINT, odbc_type, col_idx, vec, row_idx);
}

template <>
void TypeSpecific::FetchAndSetResult<int16_t>(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx,
                                              duckdb_vector vec, idx_t row_idx) {
	FetchAndSetResultInternal<int16_t>(ctx, SQL_C_SSHORT, odbc_type, col_idx, vec, row_idx);
}

template <>
void TypeSpecific::FetchAndSetResult<uint16_t>(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx,
                                               duckdb_vector vec, idx_t row_idx) {
	FetchAndSetResultInternal<uint16_t>(ctx, SQL_C_USHORT, odbc_type, col_idx, vec, row_idx);
}

template <>
void TypeSpecific::FetchAndSetResult<int32_t>(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx,
                                              duckdb_vector vec, idx_t row_idx) {
	FetchAndSetResultInternal<int32_t>(ctx, SQL_C_SLONG, odbc_type, col_idx, vec, row_idx);
}

template <>
void TypeSpecific::FetchAndSetResult<uint32_t>(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx,
                                               duckdb_vector vec, idx_t row_idx) {
	FetchAndSetResultInternal<uint32_t>(ctx, SQL_C_ULONG, odbc_type, col_idx, vec, row_idx);
}

template <>
void TypeSpecific::FetchAndSetResult<int64_t>(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx,
                                              duckdb_vector vec, idx_t row_idx) {
	FetchAndSetResultInternal<int64_t>(ctx, SQL_C_SBIGINT, odbc_type, col_idx, vec, row_idx);
}

template <>
void TypeSpecific::FetchAndSetResult<uint64_t>(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx,
                                               duckdb_vector vec, idx_t row_idx) {
	FetchAndSetResultInternal<uint64_t>(ctx, SQL_C_UBIGINT, odbc_type, col_idx, vec, row_idx);
}

template <>
duckdb_type TypeSpecific::ResolveColumnType<int8_t>(QueryContext &, ResultColumn &) {
	return DUCKDB_TYPE_TINYINT;
}

template <>
duckdb_type TypeSpecific::ResolveColumnType<uint8_t>(QueryContext &, ResultColumn &) {
	return DUCKDB_TYPE_UTINYINT;
}

template <>
duckdb_type TypeSpecific::ResolveColumnType<int16_t>(QueryContext &, ResultColumn &) {
	return DUCKDB_TYPE_SMALLINT;
}

template <>
duckdb_type TypeSpecific::ResolveColumnType<uint16_t>(QueryContext &, ResultColumn &) {
	return DUCKDB_TYPE_USMALLINT;
}

template <>
duckdb_type TypeSpecific::ResolveColumnType<int32_t>(QueryContext &, ResultColumn &) {
	return DUCKDB_TYPE_INTEGER;
}

template <>
duckdb_type TypeSpecific::ResolveColumnType<uint32_t>(QueryContext &, ResultColumn &) {
	return DUCKDB_TYPE_UINTEGER;
}

template <>
duckdb_type TypeSpecific::ResolveColumnType<int64_t>(QueryContext &, ResultColumn &) {
	return DUCKDB_TYPE_BIGINT;
}

template <>
duckdb_type TypeSpecific::ResolveColumnType<uint64_t>(QueryContext &, ResultColumn &) {
	return DUCKDB_TYPE_UBIGINT;
}

} // namespace odbcscanner
