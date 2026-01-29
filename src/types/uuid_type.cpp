#include "types.hpp"

#include <array>
#include <cstring>
#include <limits>

#include "binary.hpp"
#include "capi_pointers.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<ScannerUuid>(DbmsQuirks &, duckdb_vector vec, idx_t row_idx) {
	duckdb_uhugeint *data = reinterpret_cast<duckdb_uhugeint *>(duckdb_vector_get_data(vec));
	duckdb_uhugeint num = data[row_idx];
	num.upper ^= (std::numeric_limits<int64_t>::min)();
	ScannerUuid uuid(num);
	return ScannerValue(std::move(uuid));
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<ScannerUuid>(DbmsQuirks &, duckdb_value value) {
	duckdb_uhugeint num = duckdb_get_uuid(value);
	ScannerUuid uuid(num);
	return ScannerValue(std::move(uuid));
}

template <>
void TypeSpecific::BindOdbcParam<ScannerUuid>(QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = SQL_GUID;
	ScannerUuid &uuid = param.Value<ScannerUuid>();
	SQLRETURN ret =
	    SQLBindParameter(ctx.hstmt(), param_idx, SQL_PARAM_INPUT, SQL_C_GUID, sqltype, uuid.size<SQLULEN>(), 0,
	                     reinterpret_cast<SQLPOINTER>(uuid.data()), param.LengthBytes(), &param.LengthBytes());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' UUID failed, expected type: " + std::to_string(sqltype) +
		                       "', index: " + std::to_string(param_idx) + ", query: '" + ctx.query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

void ZeroSqlGuid(SQLGUID &guid) {
	guid.Data1 = 0;
	guid.Data2 = 0;
	guid.Data3 = 0;
	for (size_t i = 0; i < 8; i++) {
		guid.Data4[i] = 0;
	}
}

template <>
void TypeSpecific::BindColumn<ScannerUuid>(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx) {
	if (!ctx.quirks.enable_columns_binding) {
		return;
	}
	SQLGUID nguid;
	ZeroSqlGuid(nguid);
	ScannerValue nval(nguid);
	ColumnBind nbind(std::move(nval));

	ColumnBind &bind = ctx.BindForColumn(col_idx);
	bind = std::move(nbind);
	SQLGUID &fetched = bind.Value<SQLGUID>();
	SQLLEN &ind = bind.Indicator();
	SQLRETURN ret = SQLBindCol(ctx.hstmt(), col_idx, SQL_C_GUID, &fetched, sizeof(fetched), &ind);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindCol' failed, C type: " + std::to_string(SQL_C_GUID) + ", column index: " +
		                       std::to_string(col_idx) + ", column type: " + odbc_type.ToString() + ",  query: '" +
		                       ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

template <>
void TypeSpecific::FetchAndSetResult<ScannerUuid>(QueryContext &ctx, OdbcType &, SQLSMALLINT col_idx, duckdb_vector vec,

                                                  idx_t row_idx) {
	SQLGUID fetched_data;
	ZeroSqlGuid(fetched_data);
	SQLGUID *fetched_ptr = &fetched_data;
	SQLLEN ind = 0;

	if (ctx.quirks.enable_columns_binding) {
		ColumnBind &bind = ctx.BindForColumn(col_idx);
		SQLGUID &bound = bind.Value<SQLGUID>();
		fetched_ptr = &bound;
		ind = bind.ind;
	} else {
		SQLRETURN ret = SQLGetData(ctx.hstmt(), col_idx, SQL_C_GUID, &fetched_data,
		                           static_cast<SQLLEN>(sizeof(fetched_data)), &ind);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
			throw ScannerException("'SQLGetData' for UUID failed, column index: " + std::to_string(col_idx) +
			                       ", query: '" + ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" +
			                       diag + "'");
		}
	}

	SQLGUID &fetched = *fetched_ptr;

	if (ind == SQL_NULL_DATA) {
		Types::SetNullValueToResult(vec, row_idx);
		return;
	}

	duckdb_uhugeint num;

	num.upper = 0;
	num.upper |= static_cast<uint64_t>(fetched.Data1) << 32;
	num.upper |= static_cast<uint64_t>(fetched.Data2) << 16;
	num.upper |= static_cast<uint64_t>(fetched.Data3);

	num.lower = 0;
	num.lower |= static_cast<uint64_t>(fetched.Data4[0]) << 56;
	num.lower |= static_cast<uint64_t>(fetched.Data4[1]) << 48;
	num.lower |= static_cast<uint64_t>(fetched.Data4[2]) << 40;
	num.lower |= static_cast<uint64_t>(fetched.Data4[3]) << 32;
	num.lower |= static_cast<uint64_t>(fetched.Data4[4]) << 24;
	num.lower |= static_cast<uint64_t>(fetched.Data4[5]) << 16;
	num.lower |= static_cast<uint64_t>(fetched.Data4[6]) << 8;
	num.lower |= static_cast<uint64_t>(fetched.Data4[7]);

	num.upper ^= (std::numeric_limits<int64_t>::min)();

	duckdb_uhugeint *data = reinterpret_cast<duckdb_uhugeint *>(duckdb_vector_get_data(vec));
	data[row_idx] = num;
}

template <>
duckdb_type TypeSpecific::ResolveColumnType<ScannerUuid>(QueryContext &, ResultColumn &) {
	return DUCKDB_TYPE_UUID;
}

} // namespace odbcscanner