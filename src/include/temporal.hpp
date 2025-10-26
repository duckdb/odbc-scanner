#pragma once

#include <cstring>
#include <vector>

#include "duckdb_extension_api.hpp"
#include "odbc_api.hpp"

namespace odbcscanner {

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
	duckdb_timestamp_struct tss_no_micros;
	int64_t nanos_fraction = 0;

	TimestampNsStruct() {
		std::memset(&tss_no_micros, '\0', sizeof(duckdb_timestamp_struct));
	}

	TimestampNsStruct(duckdb_timestamp_struct tss_no_micros_in, int64_t nanos_fraction_in)
	    : tss_no_micros(tss_no_micros_in), nanos_fraction(nanos_fraction_in) {
	}
};

} // namespace odbcscanner