#include "types.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>

#include "capi_pointers.hpp"
#include "columns.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"
#include "temporal.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

static uint8_t DigitsCount(int64_t num) {
	if (num == 0) {
		return static_cast<uint8_t>(0);
	}
	return static_cast<uint8_t>(log10(abs(num))) + 1;
}

int64_t Pow(int64_t num, int64_t p) {
	if (p == 0) {
		return 1;
	}
	if (p == 1) {
		return num;
	}
	int64_t powed = Pow(num, p / 2);
	if (p % 2 == 0) {
		return powed * powed;
	}
	return num * powed * powed;
}

static int64_t RoundTimestampNanoFraction(DbmsQuirks &quirks, int64_t nanos_fraction) {
	uint8_t digits = DigitsCount(nanos_fraction);
	if (digits <= quirks.timestamp_max_fraction_precision) {
		return nanos_fraction;
	}
	int64_t digits_rem = static_cast<int64_t>(digits - quirks.timestamp_max_fraction_precision);
	int64_t mult = Pow(10, digits_rem);
	int64_t rem = nanos_fraction % mult;
	return nanos_fraction - rem;
}

static ScannerParam CreateParamFromDate(duckdb_date dt) {
	duckdb_date_struct dts = duckdb_from_date(dt);
	return ScannerParam(dts);
}

static ScannerParam CreateParamFromTime(DbmsQuirks &quirks, duckdb_time tm) {
	duckdb_time_struct tms = duckdb_from_time(tm);
	return ScannerParam(tms, quirks.time_params_with_nanos);
}

static ScannerParam CreateParamFromTimestamp(DbmsQuirks &quirks, duckdb_timestamp ts) {
	duckdb_timestamp_struct tss = duckdb_from_timestamp(ts);
	int64_t nanos_fraction = tss.time.micros * 1000;
	int64_t nanos_fraction_round = RoundTimestampNanoFraction(quirks, nanos_fraction);
	tss.time.micros = 0;
	TimestampNsStruct tnss(tss, nanos_fraction_round);
	return ScannerParam(tnss);
}

static ScannerParam CreateParamFromTimestampNs(DbmsQuirks &quirks, duckdb_timestamp_ns tns) {
	duckdb_timestamp ts;
	ts.micros = tns.nanos / 1000;
	int64_t nanos_remainder = tns.nanos % 1000;
	duckdb_timestamp_struct tss = duckdb_from_timestamp(ts);
	int64_t nanos_fraction = tss.time.micros * 1000 + nanos_remainder;
	int64_t nanos_fraction_round = RoundTimestampNanoFraction(quirks, nanos_fraction);
	tss.time.micros = 0;
	TimestampNsStruct tnss(tss, nanos_fraction_round);
	return ScannerParam(tnss);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<duckdb_date_struct>(DbmsQuirks &, duckdb_vector vec) {
	duckdb_date *data = reinterpret_cast<duckdb_date *>(duckdb_vector_get_data(vec));
	return CreateParamFromDate(data[0]);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<duckdb_time_struct>(DbmsQuirks &quirks, duckdb_vector vec) {
	duckdb_time *data = reinterpret_cast<duckdb_time *>(duckdb_vector_get_data(vec));
	return CreateParamFromTime(quirks, data[0]);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<duckdb_timestamp_struct>(DbmsQuirks &quirks, duckdb_vector vec) {
	duckdb_timestamp *data = reinterpret_cast<duckdb_timestamp *>(duckdb_vector_get_data(vec));
	return CreateParamFromTimestamp(quirks, data[0]);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<TimestampNsStruct>(DbmsQuirks &quirks, duckdb_vector vec) {
	duckdb_timestamp_ns *data = reinterpret_cast<duckdb_timestamp_ns *>(duckdb_vector_get_data(vec));
	return CreateParamFromTimestampNs(quirks, data[0]);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<duckdb_date_struct>(DbmsQuirks &, duckdb_value value) {
	duckdb_date dt = duckdb_get_date(value);
	return CreateParamFromDate(dt);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<duckdb_time_struct>(DbmsQuirks &quirks, duckdb_value value) {
	duckdb_time tm = duckdb_get_time(value);
	return CreateParamFromTime(quirks, tm);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<duckdb_timestamp_struct>(DbmsQuirks &quirks, duckdb_value value) {
	duckdb_timestamp ts = duckdb_get_timestamp(value);
	return CreateParamFromTimestamp(quirks, ts);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<TimestampNsStruct>(DbmsQuirks &quirks, duckdb_value value) {
	duckdb_timestamp_ns tns = duckdb_get_timestamp_ns(value);
	return CreateParamFromTimestampNs(quirks, tns);
}

template <>
void TypeSpecific::BindOdbcParam<duckdb_date_struct>(QueryContext &ctx, ScannerParam &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = SQL_TYPE_DATE;
	SQLRETURN ret = SQLBindParameter(ctx.hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_TYPE_DATE, sqltype, 0, 0,
	                                 reinterpret_cast<SQLPOINTER>(&param.Value<SQL_DATE_STRUCT>()), param.LengthBytes(),
	                                 &param.LengthBytes());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' failed, type: " + std::to_string(sqltype) +
		                       ", index: " + std::to_string(param_idx) + ", query: '" + ctx.query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

template <>
void TypeSpecific::BindOdbcParam<duckdb_time_struct>(QueryContext &ctx, ScannerParam &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = SQL_TYPE_NULL;
	SQLRETURN ret = SQL_ERROR;
	if (ctx.quirks.time_params_with_nanos) {
		sqltype = Types::SQL_SS_TIME2;
		ret = SQLBindParameter(ctx.hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_BINARY, sqltype, 0, 6,
		                       reinterpret_cast<SQLPOINTER>(&param.Value<SQL_SS_TIME2_STRUCT>()), param.LengthBytes(),
		                       &param.LengthBytes());
	} else {
		sqltype = SQL_TYPE_TIME;
		ret = SQLBindParameter(ctx.hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_TYPE_TIME, sqltype, 0, 0,
		                       reinterpret_cast<SQLPOINTER>(&param.Value<SQL_TIME_STRUCT>()), param.LengthBytes(),
		                       &param.LengthBytes());
	}
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' failed, type: " + std::to_string(sqltype) +
		                       ", index: " + std::to_string(param_idx) + ", query: '" + ctx.query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

template <>
void TypeSpecific::BindOdbcParam<duckdb_timestamp_struct>(QueryContext &ctx, ScannerParam &param,
                                                          SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = SQL_TYPE_TIMESTAMP;
	SQLRETURN ret = SQLBindParameter(ctx.hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP, sqltype, 0,
	                                 static_cast<SQLSMALLINT>(ctx.quirks.timestamp_max_fraction_precision),
	                                 reinterpret_cast<SQLPOINTER>(&param.Value<SQL_TIMESTAMP_STRUCT>()),
	                                 param.LengthBytes(), &param.LengthBytes());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' failed, type: " + std::to_string(sqltype) +
		                       ", index: " + std::to_string(param_idx) + ", query: '" + ctx.query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

template <>
void TypeSpecific::FetchAndSetResult<duckdb_date_struct>(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx,
                                                         duckdb_vector vec, idx_t row_idx) {
	SQL_DATE_STRUCT fetched;
	std::memset(&fetched, '\0', sizeof(fetched));
	SQLLEN ind;
	SQLRETURN ret = SQLGetData(ctx.hstmt, col_idx, SQL_C_TYPE_DATE, &fetched, sizeof(fetched), &ind);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' for failed, C type: " + std::to_string(SQL_C_TYPE_DATE) +
		                       ", column index: " + std::to_string(col_idx) + ", column type: " + odbc_type.ToString() +
		                       ",  query: '" + ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" +
		                       diag + "'");
	}

	if (ind == SQL_NULL_DATA) {
		Types::SetNullValueToResult(vec, row_idx);
		return;
	}

	duckdb_date_struct dts;
	std::memset(&dts, '\0', sizeof(dts));
	dts.year = static_cast<int32_t>(fetched.year);
	dts.month = static_cast<int8_t>(fetched.month);
	dts.day = static_cast<int8_t>(fetched.day);

	duckdb_date dt = duckdb_to_date(dts);

	duckdb_date *data = reinterpret_cast<duckdb_date *>(duckdb_vector_get_data(vec));
	data[row_idx] = dt;
}

static void FetchAndSetResultTime(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx, duckdb_vector vec,
                                  idx_t row_idx) {
	SQL_TIME_STRUCT fetched;
	std::memset(&fetched, '\0', sizeof(fetched));
	SQLLEN ind;
	SQLRETURN ret = SQLGetData(ctx.hstmt, col_idx, SQL_C_TYPE_TIME, &fetched, sizeof(fetched), &ind);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' for failed, C type: " + std::to_string(SQL_C_TYPE_TIME) +
		                       ", column index: " + std::to_string(col_idx) + ", column type: " + odbc_type.ToString() +
		                       ",  query: '" + ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" +
		                       diag + "'");
	}

	if (ind == SQL_NULL_DATA) {
		Types::SetNullValueToResult(vec, row_idx);
		return;
	}

	duckdb_time_struct tms;
	std::memset(&tms, '\0', sizeof(tms));
	tms.hour = static_cast<int8_t>(fetched.hour);
	tms.min = static_cast<int8_t>(fetched.minute);
	tms.sec = static_cast<int8_t>(fetched.second);

	duckdb_time tm = duckdb_to_time(tms);

	duckdb_time *data = reinterpret_cast<duckdb_time *>(duckdb_vector_get_data(vec));
	data[row_idx] = tm;
}

static void FetchAndSetResultSSTime2(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx, duckdb_vector vec,
                                     idx_t row_idx) {
	SQL_SS_TIME2_STRUCT fetched;
	std::memset(&fetched, '\0', sizeof(fetched));
	SQLLEN ind;
	SQLRETURN ret = SQLGetData(ctx.hstmt, col_idx, SQL_C_BINARY, &fetched, sizeof(fetched), &ind);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' for failed, C type: " + std::to_string(Types::SQL_SS_TIME2) +
		                       ", column index: " + std::to_string(col_idx) + ", column type: " + odbc_type.ToString() +
		                       ",  query: '" + ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" +
		                       diag + "'");
	}

	if (ind == SQL_NULL_DATA) {
		Types::SetNullValueToResult(vec, row_idx);
		return;
	}

	duckdb_time_struct tms;
	std::memset(&tms, '\0', sizeof(tms));
	tms.hour = static_cast<int8_t>(fetched.hour);
	tms.min = static_cast<int8_t>(fetched.minute);
	tms.sec = static_cast<int8_t>(fetched.second);
	tms.micros = static_cast<int32_t>(fetched.fraction / 1000);

	duckdb_time tm = duckdb_to_time(tms);

	duckdb_time *data = reinterpret_cast<duckdb_time *>(duckdb_vector_get_data(vec));
	data[row_idx] = tm;
}

template <>
void TypeSpecific::FetchAndSetResult<duckdb_time_struct>(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx,
                                                         duckdb_vector vec, idx_t row_idx) {
	switch (odbc_type.desc_concise_type) {
	case SQL_TYPE_TIME:
		FetchAndSetResultTime(ctx, odbc_type, col_idx, vec, row_idx);
		break;
	case Types::SQL_SS_TIME2:
		FetchAndSetResultSSTime2(ctx, odbc_type, col_idx, vec, row_idx);
		break;
	default:
		throw ScannerException("Unsupported ODBC TIME fetch type: " + odbc_type.ToString() + ", query: '" + ctx.query +
		                       "', column idx: " + std::to_string(col_idx));
	}
}

template <>
void TypeSpecific::FetchAndSetResult<duckdb_timestamp_struct>(QueryContext &ctx, OdbcType &odbc_type,
                                                              SQLSMALLINT col_idx, duckdb_vector vec, idx_t row_idx) {
	SQL_TIMESTAMP_STRUCT fetched;
	std::memset(&fetched, '\0', sizeof(fetched));
	SQLLEN ind;
	SQLRETURN ret = SQLGetData(ctx.hstmt, col_idx, SQL_C_TYPE_TIMESTAMP, &fetched, sizeof(fetched), &ind);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' for failed, C type: " + std::to_string(SQL_C_TYPE_TIMESTAMP) +
		                       ", column index: " + std::to_string(col_idx) + ", column type: " + odbc_type.ToString() +
		                       ",  query: '" + ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" +
		                       diag + "'");
	}

	if (ind == SQL_NULL_DATA) {
		Types::SetNullValueToResult(vec, row_idx);
		return;
	}

	duckdb_timestamp_struct tss;
	std::memset(&tss, '\0', sizeof(tss));
	tss.date.year = static_cast<int32_t>(fetched.year);
	tss.date.month = static_cast<int8_t>(fetched.month);
	tss.date.day = static_cast<int8_t>(fetched.day);
	tss.time.hour = static_cast<int8_t>(fetched.hour);
	tss.time.min = static_cast<int8_t>(fetched.minute);
	tss.time.sec = static_cast<int8_t>(fetched.second);
	tss.time.micros = static_cast<int32_t>(fetched.fraction / 1000);

	duckdb_timestamp ts = duckdb_to_timestamp(tss);

	if (ctx.quirks.datetime2_columns_as_timestamp_ns && odbc_type.desc_type_name == Types::MSSQL_DATETIME2_TYPE_NAME) {
		duckdb_timestamp_ns tns;
		tns.nanos = ts.micros * 1000;
		tns.nanos += fetched.fraction % 1000;
		duckdb_timestamp_ns *data = reinterpret_cast<duckdb_timestamp_ns *>(duckdb_vector_get_data(vec));
		data[row_idx] = tns;
	} else {
		duckdb_timestamp *data = reinterpret_cast<duckdb_timestamp *>(duckdb_vector_get_data(vec));
		data[row_idx] = ts;
	}
}

template <>
duckdb_type TypeSpecific::ResolveColumnType<duckdb_date_struct>(QueryContext &, ResultColumn &) {
	return DUCKDB_TYPE_DATE;
}

template <>
duckdb_type TypeSpecific::ResolveColumnType<duckdb_time_struct>(QueryContext &, ResultColumn &) {
	return DUCKDB_TYPE_TIME;
}

template <>
duckdb_type TypeSpecific::ResolveColumnType<duckdb_timestamp_struct>(QueryContext &ctx, ResultColumn &col) {
	if (ctx.quirks.datetime2_columns_as_timestamp_ns &&
	    col.odbc_type.desc_type_name == Types::MSSQL_DATETIME2_TYPE_NAME) {
		return DUCKDB_TYPE_TIMESTAMP_NS;
	} else {
		return DUCKDB_TYPE_TIMESTAMP;
	}
}

} // namespace odbcscanner
