#include "temporal.hpp"

namespace odbcscanner {

static const int64_t MICROS_PER_SEC = 1000000;
static const int64_t MICROS_PER_MINUTE = MICROS_PER_SEC * 60;
static const int64_t MICROS_PER_HOUR = MICROS_PER_MINUTE * 60;
static const int64_t MICROS_PER_DAY = MICROS_PER_HOUR * 24;

// Proleptic Gregorian calendar conversions, see:
// https://howardhinnant.github.io/date_algorithms.html

DateStruct Temporal::FromDate(int32_t days) {
	int64_t z = static_cast<int64_t>(days) + 719468;
	int64_t era = (z >= 0 ? z : z - 146096) / 146097;
	int64_t doe = z - era * 146097;
	int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	int64_t y = yoe + era * 400;
	int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	int64_t mp = (5 * doy + 2) / 153;
	int64_t d = doy - (153 * mp + 2) / 5 + 1;
	int64_t m = mp < 10 ? mp + 3 : mp - 9;

	DateStruct res;
	res.year = static_cast<int32_t>(y + (m <= 2 ? 1 : 0));
	res.month = static_cast<int8_t>(m);
	res.day = static_cast<int8_t>(d);
	return res;
}

int32_t Temporal::ToDate(DateStruct date) {
	int64_t m = static_cast<int64_t>(date.month);
	int64_t d = static_cast<int64_t>(date.day);
	int64_t y = static_cast<int64_t>(date.year) - (m <= 2 ? 1 : 0);
	int64_t era = (y >= 0 ? y : y - 399) / 400;
	int64_t yoe = y - era * 400;
	int64_t doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
	int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return static_cast<int32_t>(era * 146097 + doe - 719468);
}

TimeStruct Temporal::FromTime(int64_t micros) {
	TimeStruct res;
	res.hour = static_cast<int8_t>(micros / MICROS_PER_HOUR);
	micros -= static_cast<int64_t>(res.hour) * MICROS_PER_HOUR;
	res.min = static_cast<int8_t>(micros / MICROS_PER_MINUTE);
	micros -= static_cast<int64_t>(res.min) * MICROS_PER_MINUTE;
	res.sec = static_cast<int8_t>(micros / MICROS_PER_SEC);
	micros -= static_cast<int64_t>(res.sec) * MICROS_PER_SEC;
	res.micros = static_cast<int32_t>(micros);
	return res;
}

int64_t Temporal::ToTime(TimeStruct time) {
	return static_cast<int64_t>(time.hour) * MICROS_PER_HOUR + static_cast<int64_t>(time.min) * MICROS_PER_MINUTE +
	       static_cast<int64_t>(time.sec) * MICROS_PER_SEC + static_cast<int64_t>(time.micros);
}

TimestampStruct Temporal::FromTimestamp(int64_t micros) {
	// floor division, time part is always positive
	int64_t days = micros / MICROS_PER_DAY;
	if (micros < 0 && micros % MICROS_PER_DAY != 0) {
		days -= 1;
	}
	int64_t time_micros = micros - days * MICROS_PER_DAY;

	TimestampStruct res;
	res.date = FromDate(static_cast<int32_t>(days));
	res.time = FromTime(time_micros);
	return res;
}

int64_t Temporal::ToTimestamp(TimestampStruct ts) {
	int64_t days = static_cast<int64_t>(ToDate(ts.date));
	return days * MICROS_PER_DAY + ToTime(ts.time);
}

} // namespace odbcscanner
