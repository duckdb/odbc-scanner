#include "types.hpp"

#include <cstdint>
#include <cstring>

#include "capi_pointers.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<duckdb_date_struct>(duckdb_vector vec) {
	duckdb_date *data = reinterpret_cast<duckdb_date *>(duckdb_vector_get_data(vec));
	duckdb_date dt = data[0];
	duckdb_date_struct dts = duckdb_from_date(dt);
	return ScannerParam(dts);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<duckdb_time_struct>(duckdb_vector vec) {
	duckdb_time *data = reinterpret_cast<duckdb_time *>(duckdb_vector_get_data(vec));
	duckdb_time tm = data[0];
	duckdb_time_struct tms = duckdb_from_time(tm);
	return ScannerParam(tms);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<duckdb_timestamp_struct>(duckdb_vector vec) {
	duckdb_timestamp *data = reinterpret_cast<duckdb_timestamp *>(duckdb_vector_get_data(vec));
	duckdb_timestamp ts = data[0];
	duckdb_timestamp_struct tss = duckdb_from_timestamp(ts);
	return ScannerParam(tss);
}

template <>
void TypeSpecific::BindOdbcParam<duckdb_date_struct>(const std::string &query, HSTMT hstmt, ScannerParam &param,
                                                     SQLSMALLINT param_idx) {
	SQLRETURN ret = SQLBindParameter(hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_TYPE_DATE, SQL_TYPE_DATE, 0, 0,
	                                 reinterpret_cast<SQLPOINTER>(&param.Value<SQL_DATE_STRUCT>()), param.LengthBytes(),
	                                 &param.LengthBytes());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' failed, type: " + std::to_string(SQL_TYPE_DATE) +
		                       ", index: " + std::to_string(param_idx) + ", query: '" + query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

template <>
void TypeSpecific::BindOdbcParam<duckdb_time_struct>(const std::string &query, HSTMT hstmt, ScannerParam &param,
                                                     SQLSMALLINT param_idx) {
	SQLRETURN ret = SQLBindParameter(hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_TYPE_TIME, SQL_TYPE_TIME, 0, 0,
	                                 reinterpret_cast<SQLPOINTER>(&param.Value<SQL_TIME_STRUCT>()), param.LengthBytes(),
	                                 &param.LengthBytes());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' failed, type: " + std::to_string(SQL_TYPE_TIME) +
		                       ", index: " + std::to_string(param_idx) + ", query: '" + query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

template <>
void TypeSpecific::BindOdbcParam<duckdb_timestamp_struct>(const std::string &query, HSTMT hstmt, ScannerParam &param,
                                                          SQLSMALLINT param_idx) {
	SQLRETURN ret = SQLBindParameter(hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP, 0, 0,
	                                 reinterpret_cast<SQLPOINTER>(&param.Value<SQL_TIMESTAMP_STRUCT>()),
	                                 param.LengthBytes(), &param.LengthBytes());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' failed, type: " + std::to_string(SQL_TYPE_TIMESTAMP) +
		                       ", index: " + std::to_string(param_idx) + ", query: '" + query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

template <>
void TypeSpecific::FetchAndSetResult<duckdb_date_struct>(OdbcType &odbc_type, const std::string &query, HSTMT hstmt,
                                                         SQLSMALLINT col_idx, duckdb_vector vec, idx_t row_idx) {
	SQL_DATE_STRUCT fetched;
	std::memset(&fetched, '\0', sizeof(fetched));
	SQLLEN ind;
	SQLRETURN ret = SQLGetData(hstmt, col_idx, SQL_C_TYPE_DATE, &fetched, sizeof(fetched), &ind);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' for failed, C type: " + std::to_string(SQL_C_TYPE_DATE) +
		                       ", column index: " + std::to_string(col_idx) + ", column type: " + odbc_type.ToString() +
		                       ",  query: '" + query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag +
		                       "'");
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

template <>
void TypeSpecific::FetchAndSetResult<duckdb_time_struct>(OdbcType &odbc_type, const std::string &query, HSTMT hstmt,
                                                         SQLSMALLINT col_idx, duckdb_vector vec, idx_t row_idx) {
	SQL_TIME_STRUCT fetched;
	std::memset(&fetched, '\0', sizeof(fetched));
	SQLLEN ind;
	SQLRETURN ret = SQLGetData(hstmt, col_idx, SQL_C_TYPE_TIME, &fetched, sizeof(fetched), &ind);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' for failed, C type: " + std::to_string(SQL_C_TYPE_TIME) +
		                       ", column index: " + std::to_string(col_idx) + ", column type: " + odbc_type.ToString() +
		                       ",  query: '" + query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag +
		                       "'");
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

template <>
void TypeSpecific::FetchAndSetResult<duckdb_timestamp_struct>(OdbcType &odbc_type, const std::string &query,
                                                              HSTMT hstmt, SQLSMALLINT col_idx, duckdb_vector vec,
                                                              idx_t row_idx) {
	SQL_TIMESTAMP_STRUCT fetched;
	std::memset(&fetched, '\0', sizeof(fetched));
	SQLLEN ind;
	SQLRETURN ret = SQLGetData(hstmt, col_idx, SQL_C_TYPE_TIMESTAMP, &fetched, sizeof(fetched), &ind);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' for failed, C type: " + std::to_string(SQL_C_TYPE_TIMESTAMP) +
		                       ", column index: " + std::to_string(col_idx) + ", column type: " + odbc_type.ToString() +
		                       ",  query: '" + query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag +
		                       "'");
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

	duckdb_timestamp *data = reinterpret_cast<duckdb_timestamp *>(duckdb_vector_get_data(vec));
	data[row_idx] = ts;
}

} // namespace odbcscanner
