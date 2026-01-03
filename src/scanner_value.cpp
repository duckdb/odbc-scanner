#include "scanner_value.hpp"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "capi_pointers.hpp"
#include "connection.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"
#include "types.hpp"
#include "widechar.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

template <>
bool &ScannerValue::Value<bool>() {
	CheckType(DUCKDB_TYPE_BOOLEAN);
	return val.bool_val;
}

template <>
int8_t &ScannerValue::Value<int8_t>() {
	CheckType(DUCKDB_TYPE_TINYINT);
	return val.int8;
}

template <>
uint8_t &ScannerValue::Value<uint8_t>() {
	CheckType(DUCKDB_TYPE_UTINYINT);
	return val.uint8;
}

template <>
int16_t &ScannerValue::Value<int16_t>() {
	CheckType(DUCKDB_TYPE_SMALLINT);
	return val.int16;
}

template <>
uint16_t &ScannerValue::Value<uint16_t>() {
	CheckType(DUCKDB_TYPE_USMALLINT);
	return val.uint16;
}

template <>
int32_t &ScannerValue::Value<int32_t>() {
	CheckType(DUCKDB_TYPE_INTEGER);
	return val.int32;
}

template <>
uint32_t &ScannerValue::Value<uint32_t>() {
	CheckType(DUCKDB_TYPE_UINTEGER);
	return val.uint32;
}

template <>
int64_t &ScannerValue::Value<int64_t>() {
	CheckType(DUCKDB_TYPE_BIGINT);
	return val.int64;
}

template <>
uint64_t &ScannerValue::Value<uint64_t>() {
	CheckType(DUCKDB_TYPE_UBIGINT);
	return val.uint64;
}

template <>
float &ScannerValue::Value<float>() {
	CheckType(DUCKDB_TYPE_FLOAT);
	return val.float_val;
}

template <>
double &ScannerValue::Value<double>() {
	CheckType(DUCKDB_TYPE_DOUBLE);
	return val.double_val;
}

template <>
SQL_NUMERIC_STRUCT &ScannerValue::Value<SQL_NUMERIC_STRUCT>() {
	CheckType(DUCKDB_TYPE_DECIMAL);
	return val.decimal;
}

template <>
DecimalChars &ScannerValue::Value<DecimalChars>() {
	CheckType(Params::TYPE_DECIMAL_AS_CHARS);
	return val.decimal_chars;
}

template <>
WideString &ScannerValue::Value<WideString>() {
	CheckType(DUCKDB_TYPE_VARCHAR);
	return val.wstr;
}

template <>
ScannerBlob &ScannerValue::Value<ScannerBlob>() {
	CheckType(DUCKDB_TYPE_BLOB);
	return val.blob;
}

template <>
ScannerUuid &ScannerValue::Value<ScannerUuid>() {
	CheckType(DUCKDB_TYPE_UUID);
	return val.uuid;
}

template <>
SQL_DATE_STRUCT &ScannerValue::Value<SQL_DATE_STRUCT>() {
	CheckType(DUCKDB_TYPE_DATE);
	return val.date;
}

template <>
SQL_TIME_STRUCT &ScannerValue::Value<SQL_TIME_STRUCT>() {
	CheckType(DUCKDB_TYPE_TIME);
	return val.time;
}

template <>
SQL_SS_TIME2_STRUCT &ScannerValue::Value<SQL_SS_TIME2_STRUCT>() {
	CheckType(Params::TYPE_TIME_WITH_NANOS);
	return val.time_with_nanos;
}

template <>
SQL_TIMESTAMP_STRUCT &ScannerValue::Value<SQL_TIMESTAMP_STRUCT>() {
	CheckType(DUCKDB_TYPE_TIMESTAMP);
	return val.timestamp;
}

ScannerValue::ScannerValue() : type_id(DUCKDB_TYPE_SQLNULL), len_bytes(SQL_NULL_DATA) {
}

ScannerValue::ScannerValue(bool value) : type_id(DUCKDB_TYPE_BOOLEAN), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(int8_t value) : type_id(DUCKDB_TYPE_TINYINT), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(uint8_t value) : type_id(DUCKDB_TYPE_UTINYINT), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(int16_t value) : type_id(DUCKDB_TYPE_SMALLINT), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(uint16_t value) : type_id(DUCKDB_TYPE_USMALLINT), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(int32_t value) : type_id(DUCKDB_TYPE_INTEGER), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(uint32_t value) : type_id(DUCKDB_TYPE_UINTEGER), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(int64_t value) : type_id(DUCKDB_TYPE_BIGINT), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(uint64_t value) : type_id(DUCKDB_TYPE_UBIGINT), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(float value) : type_id(DUCKDB_TYPE_FLOAT), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(double value) : type_id(DUCKDB_TYPE_DOUBLE), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(duckdb_decimal value, bool decimal_as_chars) {
	if (decimal_as_chars) {
		this->type_id = Params::TYPE_DECIMAL_AS_CHARS;
		new (&this->val.decimal_chars) DecimalChars;
		this->val.decimal_chars = DecimalChars(value);
		this->len_bytes = val.decimal_chars.size<SQLLEN>();
	} else {
		this->type_id = DUCKDB_TYPE_DECIMAL;
		SQL_NUMERIC_STRUCT ns;
		std::memset(&ns, '\0', sizeof(ns));
		ns.precision = value.width;
		ns.scale = value.scale;
		ns.sign = (value.value.upper & (1LL << 63)) == 0;
		if (ns.sign == 0) {
			// negate
			value.value.lower = ~value.value.lower + 1;
			value.value.upper = ~value.value.upper;
			if (value.value.lower == 0) {
				value.value.upper += 1; // carry from low to high
			}
		}
		std::memcpy(ns.val, &value.value.lower, sizeof(value.value.lower));
		std::memcpy(ns.val + sizeof(value.value.lower), &value.value.upper, sizeof(value.value.upper));
		this->val.decimal = ns;
		this->len_bytes = sizeof(ns);
	}
}

ScannerValue::ScannerValue(SQL_NUMERIC_STRUCT value)
    : type_id(DUCKDB_TYPE_DECIMAL), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(const char *cstr, size_t len) : type_id(DUCKDB_TYPE_VARCHAR) {
	WideString wstr = WideChar::Widen(cstr, len);
	new (&this->val.wstr) WideString;
	this->val.wstr = std::move(wstr);
	this->len_bytes = val.wstr.length<SQLLEN>() * sizeof(SQLWCHAR);
}

ScannerValue::ScannerValue(ScannerBlob blob) : type_id(DUCKDB_TYPE_BLOB) {
	new (&this->val.blob) ScannerBlob;
	this->val.blob = std::move(blob);
	this->len_bytes = val.blob.size<SQLLEN>();
}

ScannerValue::ScannerValue(ScannerUuid uuid) : type_id(DUCKDB_TYPE_UUID) {
	new (&this->val.uuid) ScannerUuid;
	this->val.uuid = std::move(uuid);
	this->len_bytes = val.uuid.size<SQLLEN>();
}

ScannerValue::ScannerValue(const char *cstr) : ScannerValue(cstr, std::strlen(cstr)) {
}

ScannerValue::ScannerValue(duckdb_date_struct value) : type_id(DUCKDB_TYPE_DATE) {
	SQL_DATE_STRUCT dt;
	std::memset(&dt, '\0', sizeof(dt));
	dt.day = static_cast<SQLUSMALLINT>(value.day);
	dt.month = static_cast<SQLUSMALLINT>(value.month);
	dt.year = static_cast<SQLSMALLINT>(value.year);
	this->val.date = dt;
	this->len_bytes = sizeof(dt);
}

ScannerValue::ScannerValue(SQL_DATE_STRUCT value) : type_id(DUCKDB_TYPE_DATE), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(duckdb_time_struct value, bool use_time_with_nanos) {
	if (use_time_with_nanos) {
		this->type_id = Params::TYPE_TIME_WITH_NANOS;
		SQL_SS_TIME2_STRUCT tm;
		std::memset(&tm, '\0', sizeof(tm));
		tm.hour = static_cast<SQLUSMALLINT>(value.hour);
		tm.minute = static_cast<SQLUSMALLINT>(value.min);
		tm.second = static_cast<SQLUSMALLINT>(value.sec);
		tm.fraction = static_cast<SQLUINTEGER>(value.micros * 1000);
		this->val.time_with_nanos = tm;
		this->len_bytes = sizeof(tm);
	} else {
		this->type_id = DUCKDB_TYPE_TIME;
		SQL_TIME_STRUCT tm;
		std::memset(&tm, '\0', sizeof(tm));
		tm.hour = static_cast<SQLUSMALLINT>(value.hour);
		tm.minute = static_cast<SQLUSMALLINT>(value.min);
		tm.second = static_cast<SQLUSMALLINT>(value.sec);
		this->val.time = tm;
		this->len_bytes = sizeof(tm);
	}
}

ScannerValue::ScannerValue(TimestampNsStruct value) : type_id(DUCKDB_TYPE_TIMESTAMP) {
	SQL_TIMESTAMP_STRUCT ts;
	std::memset(&ts, '\0', sizeof(ts));
	ts.day = static_cast<SQLUSMALLINT>(value.tss_no_micros.date.day);
	ts.month = static_cast<SQLUSMALLINT>(value.tss_no_micros.date.month);
	ts.year = static_cast<SQLSMALLINT>(value.tss_no_micros.date.year);
	ts.hour = static_cast<SQLUSMALLINT>(value.tss_no_micros.time.hour);
	ts.minute = static_cast<SQLUSMALLINT>(value.tss_no_micros.time.min);
	ts.second = static_cast<SQLUSMALLINT>(value.tss_no_micros.time.sec);
	ts.fraction = static_cast<SQLUINTEGER>(value.nanos_fraction);
	this->val.timestamp = ts;
	this->len_bytes = sizeof(ts);
}

void ScannerValue::AssignByType(param_type type_id, InternalValue &val, ScannerValue &other) {
	switch (type_id) {
	case DUCKDB_TYPE_SQLNULL:
		break;
	case DUCKDB_TYPE_BOOLEAN:
		val.bool_val = other.Value<bool>();
		break;
	case DUCKDB_TYPE_TINYINT:
		val.int8 = other.Value<int8_t>();
		break;
	case DUCKDB_TYPE_UTINYINT:
		val.uint8 = other.Value<uint8_t>();
		break;
	case DUCKDB_TYPE_SMALLINT:
		val.int16 = other.Value<int16_t>();
		break;
	case DUCKDB_TYPE_USMALLINT:
		val.uint16 = other.Value<uint16_t>();
		break;
	case DUCKDB_TYPE_INTEGER:
		val.int32 = other.Value<int32_t>();
		break;
	case DUCKDB_TYPE_UINTEGER:
		val.uint32 = other.Value<uint32_t>();
		break;
	case DUCKDB_TYPE_BIGINT:
		val.int64 = other.Value<int64_t>();
		break;
	case DUCKDB_TYPE_UBIGINT:
		val.uint64 = other.Value<uint64_t>();
		break;
	case DUCKDB_TYPE_FLOAT:
		val.float_val = other.Value<float>();
		break;
	case DUCKDB_TYPE_DOUBLE:
		val.double_val = other.Value<double>();
		break;
	case DUCKDB_TYPE_DECIMAL:
		val.decimal = other.Value<SQL_NUMERIC_STRUCT>();
		break;
	case Params::TYPE_DECIMAL_AS_CHARS:
		new (&val.decimal_chars) DecimalChars;
		val.decimal_chars = std::move(other.Value<DecimalChars>());
		break;
	case DUCKDB_TYPE_VARCHAR:
		new (&val.wstr) WideString;
		val.wstr = std::move(other.Value<WideString>());
		break;
	case DUCKDB_TYPE_BLOB:
		new (&val.blob) ScannerBlob;
		val.blob = std::move(other.Value<ScannerBlob>());
		break;
	case DUCKDB_TYPE_UUID:
		new (&val.blob) ScannerUuid;
		val.uuid = std::move(other.Value<ScannerUuid>());
		break;
	case DUCKDB_TYPE_DATE:
		val.date = other.Value<SQL_DATE_STRUCT>();
		break;
	case DUCKDB_TYPE_TIME:
		val.time = other.Value<SQL_TIME_STRUCT>();
		break;
	case Params::TYPE_TIME_WITH_NANOS:
		val.time_with_nanos = other.Value<SQL_SS_TIME2_STRUCT>();
		break;
	case DUCKDB_TYPE_TIMESTAMP:
		val.timestamp = other.Value<SQL_TIMESTAMP_STRUCT>();
		break;
	default:
		throw ScannerException("Unsupported assign value type, ID: " + std::to_string(type_id));
	}
}

ScannerValue::ScannerValue(ScannerValue &&other)
    : type_id(other.type_id), len_bytes(other.len_bytes), expected_type(other.expected_type) {
	AssignByType(type_id, this->val, other);
}

ScannerValue &ScannerValue::operator=(ScannerValue &&other) {
	this->type_id = other.type_id;
	this->len_bytes = other.len_bytes;
	this->expected_type = other.expected_type;
	AssignByType(type_id, this->val, other);
	return *this;
}

ScannerValue::~ScannerValue() {
	switch (type_id) {
	case DUCKDB_TYPE_VARCHAR:
		this->val.wstr.~WideString();
		break;
	case Params::TYPE_DECIMAL_AS_CHARS:
		this->val.decimal_chars.~DecimalChars();
		break;
	default: {
		// no-op
	}
	}
}

std::string ScannerValue::ToUtf8String(size_t max_len) {
	WideString &wstr = Value<WideString>();
	size_t len = std::min(wstr.length<size_t>(), max_len);
	return WideChar::Narrow(wstr.data(), len);
}

param_type ScannerValue::ParamType() {
	return type_id;
}

duckdb_type ScannerValue::DuckType() {
	if (Params::TYPE_DECIMAL_AS_CHARS == type_id) {
		return DUCKDB_TYPE_DECIMAL;
	}
	return static_cast<duckdb_type>(type_id);
}

SQLLEN &ScannerValue::LengthBytes() {
	return len_bytes;
}

SQLSMALLINT ScannerValue::ExpectedType() {
	return expected_type;
}

void ScannerValue::SetExpectedType(SQLSMALLINT expected_type_in) {
	if (expected_type != SQL_PARAM_TYPE_UNKNOWN) {
		throw ScannerException("Value expected type is already set, value: " + std::to_string(expected_type));
	}
	this->expected_type = expected_type_in;
}

void ScannerValue::CheckType(param_type expected) {
	if (type_id != expected) {
		throw ScannerException("Value parameter type, expected: " + std::to_string(expected) +
		                       ", actual: " + std::to_string(type_id));
	}
}

void ScannerValue::TransformIntegralToDecimal() {
	if (!(type_id == DUCKDB_TYPE_TINYINT || type_id == DUCKDB_TYPE_UTINYINT || type_id == DUCKDB_TYPE_SMALLINT ||
	      type_id == DUCKDB_TYPE_USMALLINT || type_id == DUCKDB_TYPE_INTEGER || type_id == DUCKDB_TYPE_UINTEGER ||
	      type_id == DUCKDB_TYPE_BIGINT || type_id == DUCKDB_TYPE_UBIGINT)) {
		return;
	}

	duckdb_decimal dec;
	dec.width = 0;
	dec.scale = 0;
	dec.value.lower = 0;
	dec.value.upper = 0;

	switch (type_id) {
	case DUCKDB_TYPE_TINYINT:
		dec.width = 4;
		dec.value.lower = static_cast<uint64_t>(Value<int8_t>());
		break;
	case DUCKDB_TYPE_UTINYINT:
		dec.width = 4;
		dec.value.lower = static_cast<uint64_t>(Value<uint8_t>());
		break;
	case DUCKDB_TYPE_SMALLINT:
		dec.width = 9;
		dec.value.lower = static_cast<uint64_t>(Value<int16_t>());
		break;
	case DUCKDB_TYPE_USMALLINT:
		dec.width = 9;
		dec.value.lower = static_cast<uint64_t>(Value<uint16_t>());
		break;
	case DUCKDB_TYPE_INTEGER:
		dec.width = 18;
		dec.value.lower = static_cast<uint64_t>(Value<int32_t>());
		break;
	case DUCKDB_TYPE_UINTEGER:
		dec.width = 18;
		dec.value.lower = static_cast<uint64_t>(Value<uint32_t>());
		break;
	case DUCKDB_TYPE_BIGINT:
		dec.width = 38;
		dec.value.lower = static_cast<uint64_t>(Value<int64_t>());
		break;
	case DUCKDB_TYPE_UBIGINT:
		dec.width = 38;
		dec.value.lower = static_cast<uint64_t>(Value<uint64_t>());
		break;
	default:
		throw ScannerException("Invalid integral param type: " + std::to_string(type_id));
	}

	// invoke move assignment operator
	*this = ScannerValue(dec, false);
}

} // namespace odbcscanner
