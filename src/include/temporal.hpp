#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

#include "duckdb_extension_api.hpp"
#include "odbc_api.hpp"

namespace odbcscanner {

// Replaces 'duckdb_date_struct' from C API V1
struct DateStruct {
	int32_t year;
	int8_t month;
	int8_t day;
};

// Replaces 'duckdb_time_struct' from C API V1
struct TimeStruct {
	int8_t hour;
	int8_t min;
	int8_t sec;
	int32_t micros;
};

// Replaces 'duckdb_timestamp_struct' from C API V1
struct TimestampStruct {
	DateStruct date;
	TimeStruct time;
};

// Replaces date/time conversion functions from C API V1
struct Temporal {
	// days since epoch -> date struct
	static DateStruct FromDate(int32_t days);

	// date struct -> days since epoch
	static int32_t ToDate(DateStruct date);

	// microseconds since midnight -> time struct
	static TimeStruct FromTime(int64_t micros);

	// time struct -> microseconds since midnight
	static int64_t ToTime(TimeStruct time);

	// microseconds since epoch -> timestamp struct
	static TimestampStruct FromTimestamp(int64_t micros);

	// timestamp struct -> microseconds since epoch
	static int64_t ToTimestamp(TimestampStruct ts);
};

// https://learn.microsoft.com/en-us/sql/relational-databases/native-client-odbc-date-time/data-type-support-for-odbc-date-and-time-improvements?view=sql-server-2017

struct SQL_SS_TIME2_STRUCT {
	SQLUSMALLINT hour;
	SQLUSMALLINT minute;
	SQLUSMALLINT second;
	SQLUINTEGER fraction;
};

typedef struct tagSS_TIMESTAMPOFFSET_STRUCT {
	SQLSMALLINT year;
	SQLUSMALLINT month;
	SQLUSMALLINT day;
	SQLUSMALLINT hour;
	SQLUSMALLINT minute;
	SQLUSMALLINT second;
	SQLUINTEGER fraction;
	SQLSMALLINT timezone_hour;
	SQLSMALLINT timezone_minute;
} SQL_SS_TIMESTAMPOFFSET_STRUCT;

struct TimestampNsStruct {
	TimestampStruct tss_no_micros;
	int64_t nanos_fraction = 0;

	TimestampNsStruct() {
		std::memset(&tss_no_micros, '\0', sizeof(TimestampStruct));
	}

	TimestampNsStruct(TimestampStruct tss_no_micros_in, int64_t nanos_fraction_in)
	    : tss_no_micros(tss_no_micros_in), nanos_fraction(nanos_fraction_in) {
	}
};

} // namespace odbcscanner