#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <string>

#include "binary.hpp"
#include "capi_pointers.hpp"
#include "duckdb_extension_api.hpp"
#include "odbc_api.hpp"
#include "temporal.hpp"
#include "widechar.hpp"

namespace odbcscanner {

// duckdb_type with additional values
using param_type = int;

class ScannerValue {
	param_type type_id = DUCKDB_TYPE_INVALID;
	SQLLEN len_bytes = 0;
	SQLSMALLINT expected_type = SQL_PARAM_TYPE_UNKNOWN;

	union InternalValue {
		bool null_val;
		bool bool_val;
		int8_t int8;
		uint8_t uint8;
		int16_t int16;
		uint16_t uint16;
		int32_t int32;
		uint32_t uint32;
		int64_t int64;
		uint64_t uint64;
		float float_val;
		double double_val;
		SQL_NUMERIC_STRUCT decimal;
		DecimalChars decimal_chars;
		WideString wstr;
		ScannerBlob blob;
		ScannerUuid uuid;
		SQL_DATE_STRUCT date;
		SQL_TIME_STRUCT time;
		SQL_SS_TIME2_STRUCT time_with_nanos;
		SQL_TIMESTAMP_STRUCT timestamp;

		InternalValue() : null_val(true) {
		}
		InternalValue(bool value) : bool_val(value) {
		}
		InternalValue(int8_t value) : int8(value) {
		}
		InternalValue(uint8_t value) : uint8(value) {
		}
		InternalValue(int16_t value) : int16(value) {
		}
		InternalValue(uint16_t value) : uint16(value) {
		}
		InternalValue(int32_t value) : int32(value) {
		}
		InternalValue(uint32_t value) : uint32(value) {
		}
		InternalValue(int64_t value) : int64(value) {
		}
		InternalValue(uint64_t value) : uint64(value) {
		}
		InternalValue(float value) : float_val(value) {
		}
		InternalValue(double value) : double_val(value) {
		}
		InternalValue(SQL_NUMERIC_STRUCT value) : decimal(value) {
		}
		InternalValue(DecimalChars value) : decimal_chars(std::move(value)) {
		}
		InternalValue(WideString wstr_in) : wstr(std::move(wstr_in)) {
		}
		InternalValue(ScannerBlob blob_in) : blob(std::move(blob_in)) {
		}
		InternalValue(ScannerUuid uuid_in) : uuid(std::move(uuid_in)) {
		}
		InternalValue(SQL_DATE_STRUCT value) : date(value) {
		}
		InternalValue(SQL_TIME_STRUCT value) : time(value) {
		}
		InternalValue(SQL_SS_TIME2_STRUCT value) : time_with_nanos(value) {
		}
		InternalValue(SQL_TIMESTAMP_STRUCT value) : timestamp(value) {
		}

		InternalValue(InternalValue &other) = delete;
		InternalValue(InternalValue &&other) = delete;

		InternalValue &operator=(InternalValue &other) = delete;
		InternalValue &operator=(InternalValue &&other) = delete;

		~InternalValue() noexcept {};
	} val;

public:
	ScannerValue();
	explicit ScannerValue(bool value);
	explicit ScannerValue(int8_t value);
	explicit ScannerValue(uint8_t value);
	explicit ScannerValue(int16_t value);
	explicit ScannerValue(uint16_t value);
	explicit ScannerValue(int32_t value);
	explicit ScannerValue(uint32_t value);
	explicit ScannerValue(int64_t value);
	explicit ScannerValue(uint64_t value);
	explicit ScannerValue(float value);
	explicit ScannerValue(double value);
	explicit ScannerValue(duckdb_decimal value, bool decimal_as_chars);
	explicit ScannerValue(SQL_NUMERIC_STRUCT value);
	explicit ScannerValue(const char *cstr, size_t len);
	explicit ScannerValue(const char *cstr);
	explicit ScannerValue(ScannerBlob value);
	explicit ScannerValue(ScannerUuid value);
	explicit ScannerValue(duckdb_date_struct value);
	explicit ScannerValue(SQL_DATE_STRUCT value);
	explicit ScannerValue(duckdb_time_struct value, bool use_time_with_nanos);
	explicit ScannerValue(TimestampNsStruct value);

	ScannerValue(ScannerValue &other) = delete;
	ScannerValue(ScannerValue &&other);

	ScannerValue &operator=(ScannerValue &other) = delete;
	ScannerValue &operator=(ScannerValue &&other);

	~ScannerValue();

	std::string ToUtf8String(size_t max_len = std::numeric_limits<size_t>::max());

	param_type ParamType();

	duckdb_type DuckType();

	SQLLEN &LengthBytes();

	SQLSMALLINT ExpectedType();

	void SetExpectedType(SQLSMALLINT expected_type_in);

	template <typename T>
	T &Value();

	void TransformIntegralToDecimal();

private:
	void CheckType(param_type expected);

	static void AssignByType(param_type type_id, InternalValue &val, ScannerValue &other);
};

} // namespace odbcscanner