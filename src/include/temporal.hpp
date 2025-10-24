#pragma once

#include "duckdb_extension.h"

#include <cstring>

#include <vector>

#include <sql.h>
#include <sqlext.h>

namespace odbcscanner {

struct SQL_SS_TIME2_STRUCT {
	SQLUSMALLINT hour;
	SQLUSMALLINT minute;
	SQLUSMALLINT second;
	SQLUINTEGER fraction;
};

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