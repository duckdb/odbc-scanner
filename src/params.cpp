#include "params.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include "diagnostics.hpp"
#include "scanner_exception.hpp"
#include "types.hpp"
#include "widechar.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

template <>
int8_t &ScannerParam::Value<int8_t>() {
	CheckType(DUCKDB_TYPE_TINYINT);
	return val.int8;
}

template <>
uint8_t &ScannerParam::Value<uint8_t>() {
	CheckType(DUCKDB_TYPE_UTINYINT);
	return val.uint8;
}

template <>
int16_t &ScannerParam::Value<int16_t>() {
	CheckType(DUCKDB_TYPE_SMALLINT);
	return val.int16;
}

template <>
uint16_t &ScannerParam::Value<uint16_t>() {
	CheckType(DUCKDB_TYPE_USMALLINT);
	return val.uint16;
}

template <>
int32_t &ScannerParam::Value<int32_t>() {
	CheckType(DUCKDB_TYPE_INTEGER);
	return val.int32;
}

template <>
uint32_t &ScannerParam::Value<uint32_t>() {
	CheckType(DUCKDB_TYPE_UINTEGER);
	return val.uint32;
}

template <>
int64_t &ScannerParam::Value<int64_t>() {
	CheckType(DUCKDB_TYPE_BIGINT);
	return val.int64;
}

template <>
uint64_t &ScannerParam::Value<uint64_t>() {
	CheckType(DUCKDB_TYPE_UBIGINT);
	return val.uint64;
}

template <>
float &ScannerParam::Value<float>() {
	CheckType(DUCKDB_TYPE_FLOAT);
	return val.float_val;
}

template <>
double &ScannerParam::Value<double>() {
	CheckType(DUCKDB_TYPE_DOUBLE);
	return val.double_val;
}

template <>
WideString &ScannerParam::Value<WideString>() {
	CheckType(DUCKDB_TYPE_VARCHAR);
	return val.wstr;
}

template <>
SQL_DATE_STRUCT &ScannerParam::Value<SQL_DATE_STRUCT>() {
	CheckType(DUCKDB_TYPE_DATE);
	return val.date;
}

template <>
SQL_TIME_STRUCT &ScannerParam::Value<SQL_TIME_STRUCT>() {
	CheckType(DUCKDB_TYPE_TIME);
	return val.time;
}

template <>
SQL_TIMESTAMP_STRUCT &ScannerParam::Value<SQL_TIMESTAMP_STRUCT>() {
	CheckType(DUCKDB_TYPE_TIMESTAMP);
	return val.timestamp;
}

ScannerParam::ScannerParam() : type_id(DUCKDB_TYPE_SQLNULL) {
}

ScannerParam::ScannerParam(int8_t value) : type_id(DUCKDB_TYPE_TINYINT), len_bytes(sizeof(value)), val(value) {
}

ScannerParam::ScannerParam(uint8_t value) : type_id(DUCKDB_TYPE_UTINYINT), len_bytes(sizeof(value)), val(value) {
}

ScannerParam::ScannerParam(int16_t value) : type_id(DUCKDB_TYPE_SMALLINT), len_bytes(sizeof(value)), val(value) {
}

ScannerParam::ScannerParam(uint16_t value) : type_id(DUCKDB_TYPE_USMALLINT), len_bytes(sizeof(value)), val(value) {
}

ScannerParam::ScannerParam(int32_t value) : type_id(DUCKDB_TYPE_INTEGER), len_bytes(sizeof(value)), val(value) {
}

ScannerParam::ScannerParam(uint32_t value) : type_id(DUCKDB_TYPE_UINTEGER), len_bytes(sizeof(value)), val(value) {
}

ScannerParam::ScannerParam(int64_t value) : type_id(DUCKDB_TYPE_BIGINT), len_bytes(sizeof(value)), val(value) {
}

ScannerParam::ScannerParam(uint64_t value) : type_id(DUCKDB_TYPE_UBIGINT), len_bytes(sizeof(value)), val(value) {
}

ScannerParam::ScannerParam(float value) : type_id(DUCKDB_TYPE_FLOAT), len_bytes(sizeof(value)), val(value) {
}

ScannerParam::ScannerParam(double value) : type_id(DUCKDB_TYPE_DOUBLE), len_bytes(sizeof(value)), val(value) {
}

ScannerParam::ScannerParam(const char *cstr, size_t len) : type_id(DUCKDB_TYPE_VARCHAR) {
	WideString wstr = WideChar::Widen(cstr, len);
	new (&this->val.wstr) WideString;
	this->val.wstr = std::move(wstr);
	this->len_bytes = val.wstr.length<SQLLEN>() * sizeof(SQLWCHAR);
}

ScannerParam::ScannerParam(const char *cstr) : ScannerParam(cstr, std::strlen(cstr)) {
}

ScannerParam::ScannerParam(duckdb_date_struct value) : type_id(DUCKDB_TYPE_DATE) {
	SQL_DATE_STRUCT dt;
	std::memset(&dt, '\0', sizeof(dt));
	dt.day = static_cast<SQLUSMALLINT>(value.day);
	dt.month = static_cast<SQLUSMALLINT>(value.month);
	dt.year = static_cast<SQLSMALLINT>(value.year);
	this->val.date = dt;
	this->len_bytes = sizeof(dt);
}

ScannerParam::ScannerParam(duckdb_time_struct value) : type_id(DUCKDB_TYPE_TIME) {
	SQL_TIME_STRUCT tm;
	std::memset(&tm, '\0', sizeof(tm));
	tm.hour = static_cast<SQLUSMALLINT>(value.hour);
	tm.minute = static_cast<SQLUSMALLINT>(value.min);
	tm.second = static_cast<SQLUSMALLINT>(value.sec);
	this->val.time = tm;
	this->len_bytes = sizeof(tm);
}

ScannerParam::ScannerParam(duckdb_timestamp_struct value) : type_id(DUCKDB_TYPE_TIMESTAMP) {
	SQL_TIMESTAMP_STRUCT ts;
	std::memset(&ts, '\0', sizeof(ts));
	ts.day = static_cast<SQLUSMALLINT>(value.date.day);
	ts.month = static_cast<SQLUSMALLINT>(value.date.month);
	ts.year = static_cast<SQLSMALLINT>(value.date.year);
	ts.hour = static_cast<SQLUSMALLINT>(value.time.hour);
	ts.minute = static_cast<SQLUSMALLINT>(value.time.min);
	ts.second = static_cast<SQLUSMALLINT>(value.time.sec);
	ts.fraction = static_cast<SQLUINTEGER>(value.time.micros * 1000);
	this->val.timestamp = ts;
	this->len_bytes = sizeof(ts);
}

ScannerParam::ScannerParam(ScannerParam &&other) : type_id(other.type_id), len_bytes(other.len_bytes) {
	switch (type_id) {
	case DUCKDB_TYPE_SQLNULL:
		break;
	case DUCKDB_TYPE_TINYINT:
		this->val.int8 = other.Value<int8_t>();
		break;
	case DUCKDB_TYPE_UTINYINT:
		this->val.uint8 = other.Value<uint8_t>();
		break;
	case DUCKDB_TYPE_SMALLINT:
		this->val.int16 = other.Value<int16_t>();
		break;
	case DUCKDB_TYPE_USMALLINT:
		this->val.uint16 = other.Value<uint16_t>();
		break;
	case DUCKDB_TYPE_INTEGER:
		this->val.int32 = other.Value<int32_t>();
		break;
	case DUCKDB_TYPE_UINTEGER:
		this->val.uint32 = other.Value<uint32_t>();
		break;
	case DUCKDB_TYPE_BIGINT:
		this->val.int64 = other.Value<int64_t>();
		break;
	case DUCKDB_TYPE_UBIGINT:
		this->val.uint64 = other.Value<uint64_t>();
		break;
	case DUCKDB_TYPE_FLOAT:
		this->val.float_val = other.Value<float>();
		break;
	case DUCKDB_TYPE_DOUBLE:
		this->val.double_val = other.Value<double>();
		break;
	case DUCKDB_TYPE_VARCHAR:
		new (&this->val.wstr) WideString;
		this->val.wstr = std::move(other.Value<WideString>());
		break;
	case DUCKDB_TYPE_DATE:
		this->val.date = other.Value<SQL_DATE_STRUCT>();
		break;
	case DUCKDB_TYPE_TIME:
		this->val.time = other.Value<SQL_TIME_STRUCT>();
		break;
	case DUCKDB_TYPE_TIMESTAMP:
		this->val.timestamp = other.Value<SQL_TIMESTAMP_STRUCT>();
		break;
	default:
		throw ScannerException("Unsupported parameter type, ID: " + std::to_string(type_id));
	}
}

ScannerParam &ScannerParam::operator=(ScannerParam &&other) {
	this->type_id = other.type_id;
	this->len_bytes = other.len_bytes;
	switch (type_id) {
	case DUCKDB_TYPE_SQLNULL:
		break;
	case DUCKDB_TYPE_TINYINT:
		this->val.int8 = other.Value<int8_t>();
		break;
	case DUCKDB_TYPE_UTINYINT:
		this->val.uint8 = other.Value<uint8_t>();
		break;
	case DUCKDB_TYPE_SMALLINT:
		this->val.int16 = other.Value<int16_t>();
		break;
	case DUCKDB_TYPE_USMALLINT:
		this->val.uint16 = other.Value<uint16_t>();
		break;
	case DUCKDB_TYPE_INTEGER:
		this->val.int32 = other.Value<int32_t>();
		break;
	case DUCKDB_TYPE_UINTEGER:
		this->val.uint32 = other.Value<uint32_t>();
		break;
	case DUCKDB_TYPE_BIGINT:
		this->val.int64 = other.Value<int64_t>();
		break;
	case DUCKDB_TYPE_UBIGINT:
		this->val.uint64 = other.Value<uint64_t>();
		break;
	case DUCKDB_TYPE_FLOAT:
		this->val.float_val = other.Value<float>();
		break;
	case DUCKDB_TYPE_DOUBLE:
		this->val.double_val = other.Value<double>();
		break;
	case DUCKDB_TYPE_VARCHAR:
		new (&this->val.wstr) WideString;
		this->val.wstr = std::move(other.Value<WideString>());
		break;
	case DUCKDB_TYPE_DATE:
		this->val.date = other.Value<SQL_DATE_STRUCT>();
		break;
	case DUCKDB_TYPE_TIME:
		this->val.time = other.Value<SQL_TIME_STRUCT>();
		break;
	case DUCKDB_TYPE_TIMESTAMP:
		this->val.timestamp = other.Value<SQL_TIMESTAMP_STRUCT>();
		break;
	default:
		throw ScannerException("Unsupported parameter type, ID: " + std::to_string(type_id));
	}
	return *this;
}

ScannerParam::~ScannerParam() {
	switch (type_id) {
	case DUCKDB_TYPE_VARCHAR:
		this->val.wstr.~WideString();
		break;
	default: {
		// no-op
	}
	}
}

std::string ScannerParam::ToUtf8String(size_t max_len) {
	WideString &wstr = Value<WideString>();
	size_t len = std::min(wstr.length<size_t>(), max_len);
	return WideChar::Narrow(wstr.data(), len);
}

duckdb_type ScannerParam::TypeId() {
	return type_id;
}

SQLLEN &ScannerParam::LengthBytes() {
	return len_bytes;
}

void ScannerParam::CheckType(duckdb_type expected) {
	if (type_id != expected) {
		throw ScannerException("Invalid parameter type, expected: " + std::to_string(expected) +
		                       ", actual: " + std::to_string(type_id));
	}
}

std::vector<ScannerParam> Params::Extract(duckdb_data_chunk chunk, idx_t col_idx) {
	(void)col_idx;
	idx_t col_count = duckdb_data_chunk_get_column_count(chunk);
	if (col_idx >= col_count) {
		throw ScannerException("Cannot extract parameters from STRUCT: column not found, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	duckdb_vector vec = duckdb_data_chunk_get_vector(chunk, col_idx);
	if (vec == nullptr) {
		throw ScannerException("Cannot extract parameters from STRUCT: vector is NULL, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	idx_t rows_count = duckdb_data_chunk_get_size(chunk);
	if (rows_count == 0) {
		throw ScannerException("Cannot extract parameters from STRUCT: vector contains no rows, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	uint64_t *validity = duckdb_vector_get_validity(vec);
	if (validity != nullptr && !duckdb_validity_row_is_valid(validity, 0)) {
		throw ScannerException("Cannot extract parameters from STRUCT: specified STRUCT is NULL, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	auto struct_type = LogicalTypePtr(duckdb_vector_get_column_type(vec), LogicalTypeDeleter);
	duckdb_type struct_type_id = duckdb_get_type_id(struct_type.get());
	if (struct_type_id != DUCKDB_TYPE_STRUCT) {
		throw ScannerException(
		    "Cannot extract parameters from STRUCT: specified value is not STRUCT, column: " + std::to_string(col_idx) +
		    ", columns count: " + std::to_string(col_count) + ", type ID: " + std::to_string(struct_type_id));
	}

	idx_t params_count = duckdb_struct_type_child_count(struct_type.get());
	if (params_count == 0) {
		throw ScannerException("Cannot extract parameters from STRUCT: specified STRUCT has no fields, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	std::vector<ScannerParam> params;
	for (idx_t i = 0; i < params_count; i++) {
		auto child_vec = duckdb_struct_vector_get_child(vec, i);
		if (child_vec == nullptr) {
			throw ScannerException(
			    "Cannot extract parameters from STRUCT: child vector is NULL, index: " + std::to_string(i) +
			    ", column: " + std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
		}

		uint64_t *validity = duckdb_vector_get_validity(child_vec);
		if (validity != nullptr && !duckdb_validity_row_is_valid(validity, 0)) {
			params.emplace_back(ScannerParam());
			continue;
		}

		auto child_type = LogicalTypePtr(duckdb_struct_type_child_type(struct_type.get(), i), LogicalTypeDeleter);
		auto child_type_id = duckdb_get_type_id(child_type.get());
		ScannerParam sp = Types::ExtractNotNullParamOfType(child_type_id, child_vec, i);
		params.emplace_back(std::move(sp));
	}

	return params;
}

std::vector<ScannerParam> Params::Extract(duckdb_value struct_value) {
	if (duckdb_is_null_value(struct_value)) {
		throw ScannerException("Cannot extract parameters from STRUCT: specified STRUCT is NULL");
	}
	duckdb_logical_type struct_type = duckdb_get_value_type(struct_value);
	duckdb_type struct_type_id = duckdb_get_type_id(struct_type);
	if (struct_type_id != DUCKDB_TYPE_STRUCT) {
		throw ScannerException("Cannot extract parameters from STRUCT: specified value is not STRUCT");
	}
	idx_t params_count = duckdb_struct_type_child_count(struct_type);

	std::vector<ScannerParam> params;
	for (idx_t i = 0; i < params_count; i++) {
		auto child_val = ValuePtr(duckdb_get_struct_child(struct_value, i), ValueDeleter);
		if (duckdb_is_null_value(child_val.get())) {
			params.emplace_back(ScannerParam());
			continue;
		}

		ScannerParam sp = Types::ExtractNotNullParamFromValue(child_val.get(), i);
		params.emplace_back(std::move(sp));
	}

	return params;
}

std::vector<SQLSMALLINT> Params::CollectTypes(const std::string &query, HSTMT hstmt) {
	SQLSMALLINT count = -1;
	{
		SQLRETURN ret = SQLNumParams(hstmt, &count);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
			throw ScannerException("'SQLNumParams' failed, query: '" + query + "', return: " + std::to_string(ret) +
			                       ", diagnostics: '" + diag + "'");
		}
	}

	std::vector<SQLSMALLINT> param_types;
	for (SQLSMALLINT i = 0; i < count; i++) {
		SQLSMALLINT param_idx = i + 1;

		SQLSMALLINT ptype = -1;
		SQLRETURN ret = SQLDescribeParam(hstmt, param_idx, &ptype, nullptr, nullptr, nullptr);
		if (!SQL_SUCCEEDED(ret)) { // SQLDescribeParam may or may not be supported
			ptype = SQL_TYPE_NULL;
		}

		param_types.push_back(ptype);
	}

	return param_types;
}

void Params::CheckTypes(const std::string &query, const std::vector<SQLSMALLINT> &expected,
                        std::vector<ScannerParam> &actual) {
	if (expected.size() != actual.size()) {
		throw ScannerException("Incorrect number of parameters specified, query: '" + query + "', expected: " +
		                       std::to_string(expected.size()) + ", actual: " + std::to_string(actual.size()));
	}
	for (size_t i = 0; i < actual.size(); i++) {
		SQLSMALLINT expected_type = expected.at(i);
		auto &param = actual.at(i);
		SQLSMALLINT actual_type = Types::DuckParamTypeToOdbc(param.TypeId(), i);
		if (expected_type == SQL_TYPE_NULL || param.TypeId() == DUCKDB_TYPE_SQLNULL) {
			continue;
		}
		if (expected_type != actual_type) {
			throw ScannerException("Parameter ODBC type mismatch, query: '" + query + "', index: " + std::to_string(i) +
			                       ", expected: " + std::to_string(expected_type) +
			                       ", actual: " + std::to_string(actual_type));
		}
	}
}

void Params::BindToOdbc(const std::string &query, HSTMT hstmt, std::vector<ScannerParam> &params) {
	if (params.size() == 0) {
		return;
	}

	SQLRETURN ret = SQLFreeStmt(hstmt, SQL_RESET_PARAMS);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLFreeStmt' SQL_RESET_PARAMS failed, diagnostics: '" + diag + "'");
	}

	for (size_t i = 0; i < params.size(); i++) {
		ScannerParam &param = params.at(i);
		SQLSMALLINT idx = static_cast<SQLSMALLINT>(i + 1);
		Types::BindOdbcParam(query, hstmt, param, idx);
	}
}

} // namespace odbcscanner
