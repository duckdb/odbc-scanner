#include "scanner_value.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "function_args.hpp"
#include "connection.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"
#include "types.hpp"
#include "widechar.hpp"

namespace odbcscanner {

template <>
bool &ScannerValue::Value<bool>() {
	CheckType(ToParamType(LogicalTypeId::BOOLEAN));
	return val.bool_val;
}

template <>
int8_t &ScannerValue::Value<int8_t>() {
	CheckType(ToParamType(LogicalTypeId::TINYINT));
	return val.int8;
}

template <>
uint8_t &ScannerValue::Value<uint8_t>() {
	CheckType(ToParamType(LogicalTypeId::UTINYINT));
	return val.uint8;
}

template <>
int16_t &ScannerValue::Value<int16_t>() {
	CheckType(ToParamType(LogicalTypeId::SMALLINT));
	return val.int16;
}

template <>
uint16_t &ScannerValue::Value<uint16_t>() {
	CheckType(ToParamType(LogicalTypeId::USMALLINT));
	return val.uint16;
}

template <>
int32_t &ScannerValue::Value<int32_t>() {
	CheckType(ToParamType(LogicalTypeId::INTEGER));
	return val.int32;
}

template <>
uint32_t &ScannerValue::Value<uint32_t>() {
	CheckType(ToParamType(LogicalTypeId::UINTEGER));
	return val.uint32;
}

template <>
int64_t &ScannerValue::Value<int64_t>() {
	CheckType(ToParamType(LogicalTypeId::BIGINT));
	return val.int64;
}

template <>
uint64_t &ScannerValue::Value<uint64_t>() {
	CheckType(ToParamType(LogicalTypeId::UBIGINT));
	return val.uint64;
}

template <>
float &ScannerValue::Value<float>() {
	CheckType(ToParamType(LogicalTypeId::FLOAT));
	return val.float_val;
}

template <>
double &ScannerValue::Value<double>() {
	CheckType(ToParamType(LogicalTypeId::DOUBLE));
	return val.double_val;
}

template <>
SQL_NUMERIC_STRUCT &ScannerValue::Value<SQL_NUMERIC_STRUCT>() {
	CheckType(ToParamType(LogicalTypeId::DECIMAL));
	return val.decimal;
}

template <>
DecimalChars &ScannerValue::Value<DecimalChars>() {
	CheckType(Params::TYPE_DECIMAL_AS_CHARS);
	return val.decimal_chars;
}

template <>
WideString &ScannerValue::Value<WideString>() {
	CheckType(ToParamType(LogicalTypeId::VARCHAR));
	return val.wstr;
}

template <>
ScannerBlob &ScannerValue::Value<ScannerBlob>() {
	CheckType(ToParamType(LogicalTypeId::BLOB));
	return val.blob;
}

template <>
ScannerUuid &ScannerValue::Value<ScannerUuid>() {
	CheckType(ToParamType(LogicalTypeId::UUID));
	return val.uuid;
}

template <>
SQL_DATE_STRUCT &ScannerValue::Value<SQL_DATE_STRUCT>() {
	CheckType(ToParamType(LogicalTypeId::DATE));
	return val.date;
}

template <>
SQL_TIME_STRUCT &ScannerValue::Value<SQL_TIME_STRUCT>() {
	CheckType(ToParamType(LogicalTypeId::TIME));
	return val.time;
}

template <>
SQL_SS_TIME2_STRUCT &ScannerValue::Value<SQL_SS_TIME2_STRUCT>() {
	CheckType(Params::TYPE_TIME_WITH_NANOS);
	return val.time_with_nanos;
}

template <>
SQL_TIMESTAMP_STRUCT &ScannerValue::Value<SQL_TIMESTAMP_STRUCT>() {
	CheckType(ToParamType(LogicalTypeId::TIMESTAMP));
	return val.timestamp;
}

template <>
SQL_SS_TIMESTAMPOFFSET_STRUCT &ScannerValue::Value<SQL_SS_TIMESTAMPOFFSET_STRUCT>() {
	CheckType(Params::TYPE_SS_TIMESTAMPOFFSET);
	return val.timestamp_offset;
}

template <>
SqlBit &ScannerValue::Value<SqlBit>() {
	CheckType(Params::TYPE_SQL_BIT);
	return val.sql_bit;
}

template <>
SQLGUID &ScannerValue::Value<SQLGUID>() {
	CheckType(Params::TYPE_SQL_GUID);
	return val.sql_guid;
}

ScannerValue::ScannerValue() : type_id(ToParamType(LogicalTypeId::SQLNULL)), len_bytes(SQL_NULL_DATA) {
}

ScannerValue::ScannerValue(bool value)
    : type_id(ToParamType(LogicalTypeId::BOOLEAN)), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(int8_t value)
    : type_id(ToParamType(LogicalTypeId::TINYINT)), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(uint8_t value)
    : type_id(ToParamType(LogicalTypeId::UTINYINT)), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(int16_t value)
    : type_id(ToParamType(LogicalTypeId::SMALLINT)), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(uint16_t value)
    : type_id(ToParamType(LogicalTypeId::USMALLINT)), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(int32_t value)
    : type_id(ToParamType(LogicalTypeId::INTEGER)), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(uint32_t value)
    : type_id(ToParamType(LogicalTypeId::UINTEGER)), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(int64_t value)
    : type_id(ToParamType(LogicalTypeId::BIGINT)), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(uint64_t value)
    : type_id(ToParamType(LogicalTypeId::UBIGINT)), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(float value)
    : type_id(ToParamType(LogicalTypeId::FLOAT)), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(double value)
    : type_id(ToParamType(LogicalTypeId::DOUBLE)), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(DecimalValue value, bool decimal_as_chars) {
	if (decimal_as_chars) {
		this->type_id = Params::TYPE_DECIMAL_AS_CHARS;
		new (&this->val.decimal_chars) DecimalChars;
		this->val.decimal_chars = DecimalChars(value);
		this->len_bytes = val.decimal_chars.size<SQLLEN>();
	} else {
		this->type_id = ToParamType(LogicalTypeId::DECIMAL);
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
    : type_id(ToParamType(LogicalTypeId::DECIMAL)), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(const char *cstr, size_t len) : type_id(ToParamType(LogicalTypeId::VARCHAR)) {
	WideString wstr = WideChar::Widen(cstr, len);
	new (&this->val.wstr) WideString;
	this->val.wstr = std::move(wstr);
	this->len_bytes = val.wstr.length<SQLLEN>() * sizeof(SQLWCHAR);
}

ScannerValue::ScannerValue(ScannerBlob blob) : type_id(ToParamType(LogicalTypeId::BLOB)) {
	new (&this->val.blob) ScannerBlob;
	this->val.blob = std::move(blob);
	this->len_bytes = val.blob.size<SQLLEN>();
}

ScannerValue::ScannerValue(ScannerUuid uuid) : type_id(ToParamType(LogicalTypeId::UUID)) {
	new (&this->val.uuid) ScannerUuid;
	this->val.uuid = std::move(uuid);
	this->len_bytes = val.uuid.size<SQLLEN>();
}

ScannerValue::ScannerValue(const char *cstr) : ScannerValue(cstr, std::strlen(cstr)) {
}

ScannerValue::ScannerValue(DateStruct value) : type_id(ToParamType(LogicalTypeId::DATE)) {
	SQL_DATE_STRUCT dt;
	std::memset(&dt, '\0', sizeof(dt));
	dt.day = static_cast<SQLUSMALLINT>(value.day);
	dt.month = static_cast<SQLUSMALLINT>(value.month);
	dt.year = static_cast<SQLSMALLINT>(value.year);
	this->val.date = dt;
	this->len_bytes = sizeof(dt);
}

ScannerValue::ScannerValue(SQL_DATE_STRUCT value)
    : type_id(ToParamType(LogicalTypeId::DATE)), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(TimeStruct value, bool use_time_with_nanos) {
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
		this->type_id = ToParamType(LogicalTypeId::TIME);
		SQL_TIME_STRUCT tm;
		std::memset(&tm, '\0', sizeof(tm));
		tm.hour = static_cast<SQLUSMALLINT>(value.hour);
		tm.minute = static_cast<SQLUSMALLINT>(value.min);
		tm.second = static_cast<SQLUSMALLINT>(value.sec);
		this->val.time = tm;
		this->len_bytes = sizeof(tm);
	}
}

ScannerValue::ScannerValue(SQL_TIME_STRUCT value)
    : type_id(ToParamType(LogicalTypeId::TIME)), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(SQL_SS_TIME2_STRUCT value)
    : type_id(Params::TYPE_TIME_WITH_NANOS), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(TimestampNsStruct value) : type_id(ToParamType(LogicalTypeId::TIMESTAMP)) {
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

ScannerValue::ScannerValue(SQL_TIMESTAMP_STRUCT value)
    : type_id(ToParamType(LogicalTypeId::TIMESTAMP)), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(SQL_SS_TIMESTAMPOFFSET_STRUCT value)
    : type_id(Params::TYPE_SS_TIMESTAMPOFFSET), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(SqlBit value) : type_id(Params::TYPE_SQL_BIT), len_bytes(sizeof(value)), val(value) {
}

ScannerValue::ScannerValue(SQLGUID value) : type_id(Params::TYPE_SQL_GUID), len_bytes(sizeof(value)), val(value) {
}

void ScannerValue::AssignByType(param_type type_id, InternalValue &val, ScannerValue &other) {
	switch (type_id) {
	case ToParamType(LogicalTypeId::SQLNULL):
		break;
	case ToParamType(LogicalTypeId::BOOLEAN):
		val.bool_val = other.Value<bool>();
		break;
	case ToParamType(LogicalTypeId::TINYINT):
		val.int8 = other.Value<int8_t>();
		break;
	case ToParamType(LogicalTypeId::UTINYINT):
		val.uint8 = other.Value<uint8_t>();
		break;
	case ToParamType(LogicalTypeId::SMALLINT):
		val.int16 = other.Value<int16_t>();
		break;
	case ToParamType(LogicalTypeId::USMALLINT):
		val.uint16 = other.Value<uint16_t>();
		break;
	case ToParamType(LogicalTypeId::INTEGER):
		val.int32 = other.Value<int32_t>();
		break;
	case ToParamType(LogicalTypeId::UINTEGER):
		val.uint32 = other.Value<uint32_t>();
		break;
	case ToParamType(LogicalTypeId::BIGINT):
		val.int64 = other.Value<int64_t>();
		break;
	case ToParamType(LogicalTypeId::UBIGINT):
		val.uint64 = other.Value<uint64_t>();
		break;
	case ToParamType(LogicalTypeId::FLOAT):
		val.float_val = other.Value<float>();
		break;
	case ToParamType(LogicalTypeId::DOUBLE):
		val.double_val = other.Value<double>();
		break;
	case ToParamType(LogicalTypeId::DECIMAL):
		val.decimal = other.Value<SQL_NUMERIC_STRUCT>();
		break;
	case Params::TYPE_DECIMAL_AS_CHARS:
		new (&val.decimal_chars) DecimalChars;
		val.decimal_chars = std::move(other.Value<DecimalChars>());
		break;
	case ToParamType(LogicalTypeId::VARCHAR):
		new (&val.wstr) WideString;
		val.wstr = std::move(other.Value<WideString>());
		break;
	case ToParamType(LogicalTypeId::BLOB):
		new (&val.blob) ScannerBlob;
		val.blob = std::move(other.Value<ScannerBlob>());
		break;
	case ToParamType(LogicalTypeId::UUID):
		new (&val.uuid) ScannerUuid;
		val.uuid = std::move(other.Value<ScannerUuid>());
		break;
	case ToParamType(LogicalTypeId::DATE):
		val.date = other.Value<SQL_DATE_STRUCT>();
		break;
	case ToParamType(LogicalTypeId::TIME):
		val.time = other.Value<SQL_TIME_STRUCT>();
		break;
	case Params::TYPE_TIME_WITH_NANOS:
		val.time_with_nanos = other.Value<SQL_SS_TIME2_STRUCT>();
		break;
	case ToParamType(LogicalTypeId::TIMESTAMP):
		val.timestamp = other.Value<SQL_TIMESTAMP_STRUCT>();
		break;
	case Params::TYPE_SS_TIMESTAMPOFFSET:
		val.timestamp_offset = other.Value<SQL_SS_TIMESTAMPOFFSET_STRUCT>();
		break;
	case Params::TYPE_SQL_BIT:
		val.sql_bit = other.Value<SqlBit>();
		break;
	case Params::TYPE_SQL_GUID:
		val.sql_guid = other.Value<SQLGUID>();
		break;
	default:
		throw ScannerException("Unsupported assign value type, ID: " + std::to_string(type_id));
	}
}

void ScannerValue::Destroy() {
	switch (type_id) {
	case ToParamType(LogicalTypeId::VARCHAR):
		this->val.wstr.~WideString();
		break;
	case Params::TYPE_DECIMAL_AS_CHARS:
		this->val.decimal_chars.~DecimalChars();
		break;
	case ToParamType(LogicalTypeId::BLOB):
		this->val.blob.~ScannerBlob();
		break;
	case ToParamType(LogicalTypeId::UUID):
		this->val.uuid.~ScannerUuid();
		break;
	default: {
		// no-op
	}
	}
}

ScannerValue::ScannerValue(ScannerValue &&other)
    : type_id(other.type_id), len_bytes(other.len_bytes), expected_type(other.expected_type) {
	AssignByType(type_id, this->val, other);
}

ScannerValue &ScannerValue::operator=(ScannerValue &&other) {
	this->Destroy();
	this->type_id = other.type_id;
	this->len_bytes = other.len_bytes;
	this->expected_type = other.expected_type;
	AssignByType(type_id, this->val, other);
	return *this;
}

ScannerValue::~ScannerValue() {
	this->Destroy();
}

std::string ScannerValue::ToUtf8String(size_t max_len) {
	WideString &wstr = Value<WideString>();
	size_t len = std::min(wstr.length<size_t>(), max_len);
	return WideChar::Narrow(wstr.data(), len);
}

param_type ScannerValue::ParamType() {
	return type_id;
}

LogicalTypeId ScannerValue::DuckType() {
	if (Params::TYPE_DECIMAL_AS_CHARS == type_id) {
		return LogicalTypeId::DECIMAL;
	}
	return static_cast<LogicalTypeId>(type_id);
}

SQLLEN &ScannerValue::LengthBytes() {
	return len_bytes;
}

void ScannerValue::SetLengthBytes(SQLLEN value) {
	this->len_bytes = value;
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
	if (!(type_id == ToParamType(LogicalTypeId::TINYINT) || type_id == ToParamType(LogicalTypeId::UTINYINT) ||
	      type_id == ToParamType(LogicalTypeId::SMALLINT) || type_id == ToParamType(LogicalTypeId::USMALLINT) ||
	      type_id == ToParamType(LogicalTypeId::INTEGER) || type_id == ToParamType(LogicalTypeId::UINTEGER) ||
	      type_id == ToParamType(LogicalTypeId::BIGINT) || type_id == ToParamType(LogicalTypeId::UBIGINT))) {
		return;
	}

	DecimalValue dec;
	dec.width = 0;
	dec.scale = 0;
	dec.value.lower = 0;
	dec.value.upper = 0;

	switch (type_id) {
	case ToParamType(LogicalTypeId::TINYINT):
		dec.width = 4;
		dec.value.lower = static_cast<uint64_t>(Value<int8_t>());
		break;
	case ToParamType(LogicalTypeId::UTINYINT):
		dec.width = 4;
		dec.value.lower = static_cast<uint64_t>(Value<uint8_t>());
		break;
	case ToParamType(LogicalTypeId::SMALLINT):
		dec.width = 9;
		dec.value.lower = static_cast<uint64_t>(Value<int16_t>());
		break;
	case ToParamType(LogicalTypeId::USMALLINT):
		dec.width = 9;
		dec.value.lower = static_cast<uint64_t>(Value<uint16_t>());
		break;
	case ToParamType(LogicalTypeId::INTEGER):
		dec.width = 18;
		dec.value.lower = static_cast<uint64_t>(Value<int32_t>());
		break;
	case ToParamType(LogicalTypeId::UINTEGER):
		dec.width = 18;
		dec.value.lower = static_cast<uint64_t>(Value<uint32_t>());
		break;
	case ToParamType(LogicalTypeId::BIGINT):
		dec.width = 38;
		dec.value.lower = static_cast<uint64_t>(Value<int64_t>());
		break;
	case ToParamType(LogicalTypeId::UBIGINT):
		dec.width = 38;
		dec.value.lower = static_cast<uint64_t>(Value<uint64_t>());
		break;
	default:
		throw ScannerException("Invalid integral param type: " + std::to_string(type_id));
	}

	// invoke move assignment operator
	*this = ScannerValue(dec, false);
}

std::string ScannerValue::NumericToString() {
	switch (type_id) {
	case ToParamType(LogicalTypeId::TINYINT):
		return std::to_string(static_cast<int32_t>(Value<int8_t>()));
	case ToParamType(LogicalTypeId::UTINYINT):
		return std::to_string(static_cast<uint32_t>(Value<uint8_t>()));
	case ToParamType(LogicalTypeId::SMALLINT):
		return std::to_string(Value<int16_t>());
	case ToParamType(LogicalTypeId::USMALLINT):
		return std::to_string(Value<uint16_t>());
	case ToParamType(LogicalTypeId::INTEGER):
		return std::to_string(Value<int32_t>());
	case ToParamType(LogicalTypeId::UINTEGER):
		return std::to_string(Value<uint32_t>());
	case ToParamType(LogicalTypeId::BIGINT):
		return std::to_string(Value<int64_t>());
	case ToParamType(LogicalTypeId::UBIGINT):
		return std::to_string(Value<uint64_t>());
	case ToParamType(LogicalTypeId::FLOAT): {
		char buf[32];
		std::snprintf(buf, sizeof(buf), "%.9g", static_cast<double>(Value<float>()));
		return std::string(buf);
	}
	case ToParamType(LogicalTypeId::DOUBLE): {
		char buf[32];
		std::snprintf(buf, sizeof(buf), "%.17g", Value<double>());
		return std::string(buf);
	}
	default:
		throw ScannerException("Invalid numeric param type for chars transform: " + std::to_string(type_id));
	}
}

void ScannerValue::TransformNumericToChars() {
	std::string str = NumericToString();

	// Destroy the current (POD) value and repurpose the union as DecimalChars.
	// Wide character targets are handled downstream by BindOdbcParam<DecimalChars>.
	this->Destroy();
	this->type_id = Params::TYPE_DECIMAL_AS_CHARS;
	new (&this->val.decimal_chars) DecimalChars;
	DecimalChars &dc = this->val.decimal_chars;
	dc.characters.resize(str.size() + 1);
	std::memcpy(dc.characters.data(), str.data(), str.size());
	dc.characters[str.size()] = '\0';
	this->len_bytes = static_cast<SQLLEN>(str.size());
}

} // namespace odbcscanner
