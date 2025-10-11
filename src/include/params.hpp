#pragma once

#include "duckdb_extension.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <sql.h>
#include <sqlext.h>

#include "capi_pointers.hpp"
#include "query_context.hpp"
#include "widechar.hpp"

namespace odbcscanner {

// duckdb_type with additional values
using param_type = int;

struct DecimalChars {
	std::vector<char> characters;

	DecimalChars();

	explicit DecimalChars(duckdb_decimal decimal);

	DecimalChars(DecimalChars &other) = delete;
	DecimalChars(DecimalChars &&other) = default;

	DecimalChars &operator=(const DecimalChars &other) = delete;
	DecimalChars &operator=(DecimalChars &&other) = default;

	template <typename INT_TYPE>
	INT_TYPE size() {
		return static_cast<INT_TYPE>(characters.size() - 1);
	}

	char *data();
};

class ScannerParam {
	param_type type_id = DUCKDB_TYPE_INVALID;
	SQLLEN len_bytes = 0;
	SQLSMALLINT expected_type = SQL_PARAM_TYPE_UNKNOWN;

	union InternalValue {
		bool null_val;
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
		SQL_DATE_STRUCT date;
		SQL_TIME_STRUCT time;
		SQL_TIMESTAMP_STRUCT timestamp;

		InternalValue() : null_val(true) {
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
		InternalValue(SQL_DATE_STRUCT value) : date(value) {
		}
		InternalValue(SQL_TIME_STRUCT value) : time(value) {
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
	ScannerParam();
	explicit ScannerParam(int8_t value);
	explicit ScannerParam(uint8_t value);
	explicit ScannerParam(int16_t value);
	explicit ScannerParam(uint16_t value);
	explicit ScannerParam(int32_t value);
	explicit ScannerParam(uint32_t value);
	explicit ScannerParam(int64_t value);
	explicit ScannerParam(uint64_t value);
	explicit ScannerParam(float value);
	explicit ScannerParam(double value);
	explicit ScannerParam(duckdb_decimal value, bool decimal_as_chars);
	explicit ScannerParam(const char *cstr, size_t len);
	explicit ScannerParam(const char *cstr);
	explicit ScannerParam(duckdb_date_struct value);
	explicit ScannerParam(duckdb_time_struct value);
	explicit ScannerParam(duckdb_timestamp_struct value);

	ScannerParam(ScannerParam &other) = delete;
	ScannerParam(ScannerParam &&other);

	ScannerParam &operator=(ScannerParam &other) = delete;
	ScannerParam &operator=(ScannerParam &&other);

	~ScannerParam();

	std::string ToUtf8String(size_t max_len = std::numeric_limits<size_t>::max());

	param_type ParamType();

	duckdb_type DuckType();

	SQLLEN &LengthBytes();

	SQLSMALLINT ExpectedType();

	void SetExpectedType(SQLSMALLINT expected_type_in);

	template <typename T>
	T &Value();

private:
	void CheckType(param_type expected);

	static void AssignByType(param_type type_id, InternalValue &val, ScannerParam &other);
};

struct Params {
	static const param_type TYPE_DECIMAL_AS_CHARS = DUCKDB_TYPE_DECIMAL + 1000;

	static std::vector<ScannerParam> Extract(DbmsQuirks &quirks, duckdb_data_chunk chunk, idx_t col_idx);

	static std::vector<ScannerParam> Extract(DbmsQuirks &quirks, duckdb_value struct_value);

	static std::vector<SQLSMALLINT> CollectTypes(QueryContext &ctx);

	static void SetExpectedTypes(QueryContext &ctx, const std::vector<SQLSMALLINT> &expected,
	                             std::vector<ScannerParam> &actual);

	static void BindToOdbc(QueryContext &ctx, std::vector<ScannerParam> &params);
};

} // namespace odbcscanner
