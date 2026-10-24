#include "types.hpp"

#include <cstdint>
#include <vector>

#include "columns.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"
#include "temporal.hpp"
#include "widechar.hpp"

namespace odbcscanner {

std::string OdbcType::ToString() const {
	return "code: " + std::to_string(desc_type) + ", concise type: " + std::to_string(desc_concise_type) +
	       ", type name: '" + desc_type_name + "', unsigned: " + std::to_string(is_unsigned) +
	       ", precision: " + std::to_string(decimal_precision) + ", scale: " + std::to_string(decimal_scale);
}

bool OdbcType::Equals(const OdbcType &other) const {
	return desc_type == other.desc_type && desc_concise_type == other.desc_concise_type &&
	       desc_type_name == other.desc_type_name && is_unsigned == other.is_unsigned &&
	       decimal_precision == other.decimal_precision && decimal_scale == other.decimal_scale;
}

const std::string Types::UNKNOWN_DUCKDB_TYPE_NAME = "DUCKDB_TYPE_UNKNOWN";
const std::string Types::MSSQL_DATETIME2_TYPE_NAME = "datetime2";
const std::string Types::SQL_DATE_TYPE_NAME = "DATE";
const std::string Types::SQL_BLOB_TYPE_NAME = "BLOB";
const std::string Types::SQL_CLOB_TYPE_NAME = "CLOB";
const std::string Types::SQL_DB2_DBCLOB_TYPE_NAME = "DBCLOB";

bool Types::IsCharacterSQLType(SQLSMALLINT t) {
	return t == SQL_CHAR || t == SQL_VARCHAR || t == SQL_LONGVARCHAR || t == SQL_WCHAR || t == SQL_WVARCHAR ||
	       t == SQL_WLONGVARCHAR;
}

bool Types::IsWideCharacterSQLType(SQLSMALLINT t) {
	return t == SQL_WCHAR || t == SQL_WVARCHAR || t == SQL_WLONGVARCHAR;
}

ScannerValue Types::ExtractNotNullParam(const DbmsQuirks &quirks, LogicalTypeId type_id, Vector &vec, idx_t row_idx,
                                        idx_t param_idx) {
	switch (type_id) {
	case LogicalTypeId::BOOLEAN:
		return TypeSpecific::ExtractNotNullParam<bool>(quirks, vec, row_idx);
	case LogicalTypeId::TINYINT:
		return TypeSpecific::ExtractNotNullParam<int8_t>(quirks, vec, row_idx);
	case LogicalTypeId::UTINYINT:
		return TypeSpecific::ExtractNotNullParam<uint8_t>(quirks, vec, row_idx);
	case LogicalTypeId::SMALLINT:
		return TypeSpecific::ExtractNotNullParam<int16_t>(quirks, vec, row_idx);
	case LogicalTypeId::USMALLINT:
		return TypeSpecific::ExtractNotNullParam<uint16_t>(quirks, vec, row_idx);
	case LogicalTypeId::INTEGER:
		return TypeSpecific::ExtractNotNullParam<int32_t>(quirks, vec, row_idx);
	case LogicalTypeId::UINTEGER:
		return TypeSpecific::ExtractNotNullParam<uint32_t>(quirks, vec, row_idx);
	case LogicalTypeId::BIGINT:
		return TypeSpecific::ExtractNotNullParam<int64_t>(quirks, vec, row_idx);
	case LogicalTypeId::UBIGINT:
		return TypeSpecific::ExtractNotNullParam<uint64_t>(quirks, vec, row_idx);
	case LogicalTypeId::FLOAT:
		return TypeSpecific::ExtractNotNullParam<float>(quirks, vec, row_idx);
	case LogicalTypeId::DOUBLE:
		return TypeSpecific::ExtractNotNullParam<double>(quirks, vec, row_idx);
	case LogicalTypeId::DECIMAL:
		return TypeSpecific::ExtractNotNullParam<DecimalValue>(quirks, vec, row_idx);
	case LogicalTypeId::VARCHAR:
		return TypeSpecific::ExtractNotNullParam<std::string>(quirks, vec, row_idx);
	case LogicalTypeId::BLOB:
		return TypeSpecific::ExtractNotNullParam<ScannerBlob>(quirks, vec, row_idx);
	case LogicalTypeId::UUID:
		return TypeSpecific::ExtractNotNullParam<ScannerUuid>(quirks, vec, row_idx);
	case LogicalTypeId::DATE:
		return TypeSpecific::ExtractNotNullParam<DateStruct>(quirks, vec, row_idx);
	case LogicalTypeId::TIME:
		return TypeSpecific::ExtractNotNullParam<TimeStruct>(quirks, vec, row_idx);
	case LogicalTypeId::TIMESTAMP:
	case LogicalTypeId::TIMESTAMP_TZ:
		return TypeSpecific::ExtractNotNullParam<TimestampStruct>(quirks, vec, row_idx);
	case LogicalTypeId::TIMESTAMP_NS:
		return TypeSpecific::ExtractNotNullParam<TimestampNsStruct>(quirks, vec, row_idx);
	default:
		throw ScannerException("Cannot extract parameters from STRUCT: specified type is not supported, id: " +
		                       std::to_string(static_cast<uint32_t>(type_id)) +
		                       ", index: " + std::to_string(param_idx));
	}
}

ScannerValue Types::ExtractNotNullParam(const DbmsQuirks &quirks, const Value *value, idx_t param_idx) {
	auto type_id = Types::TypeIdOf(value->GetLogicalType());
	switch (type_id) {
	case LogicalTypeId::BOOLEAN:
		return TypeSpecific::ExtractNotNullParam<bool>(quirks, value);
	case LogicalTypeId::TINYINT:
		return TypeSpecific::ExtractNotNullParam<int8_t>(quirks, value);
	case LogicalTypeId::UTINYINT:
		return TypeSpecific::ExtractNotNullParam<uint8_t>(quirks, value);
	case LogicalTypeId::SMALLINT:
		return TypeSpecific::ExtractNotNullParam<int16_t>(quirks, value);
	case LogicalTypeId::USMALLINT:
		return TypeSpecific::ExtractNotNullParam<uint16_t>(quirks, value);
	case LogicalTypeId::INTEGER:
		return TypeSpecific::ExtractNotNullParam<int32_t>(quirks, value);
	case LogicalTypeId::UINTEGER:
		return TypeSpecific::ExtractNotNullParam<uint32_t>(quirks, value);
	case LogicalTypeId::BIGINT:
		return TypeSpecific::ExtractNotNullParam<int64_t>(quirks, value);
	case LogicalTypeId::UBIGINT:
		return TypeSpecific::ExtractNotNullParam<uint64_t>(quirks, value);
	case LogicalTypeId::FLOAT:
		return TypeSpecific::ExtractNotNullParam<float>(quirks, value);
	case LogicalTypeId::DOUBLE:
		return TypeSpecific::ExtractNotNullParam<double>(quirks, value);
	case LogicalTypeId::DECIMAL:
		return TypeSpecific::ExtractNotNullParam<DecimalValue>(quirks, value);
	case LogicalTypeId::VARCHAR:
		return TypeSpecific::ExtractNotNullParam<std::string>(quirks, value);
	case LogicalTypeId::BLOB:
		return TypeSpecific::ExtractNotNullParam<ScannerBlob>(quirks, value);
	case LogicalTypeId::UUID:
		return TypeSpecific::ExtractNotNullParam<ScannerUuid>(quirks, value);
	case LogicalTypeId::DATE:
		return TypeSpecific::ExtractNotNullParam<DateStruct>(quirks, value);
	case LogicalTypeId::TIME:
		return TypeSpecific::ExtractNotNullParam<TimeStruct>(quirks, value);
	case LogicalTypeId::TIMESTAMP:
	case LogicalTypeId::TIMESTAMP_TZ:
		return TypeSpecific::ExtractNotNullParam<TimestampStruct>(quirks, value);
	case LogicalTypeId::TIMESTAMP_NS:
		return TypeSpecific::ExtractNotNullParam<TimestampNsStruct>(quirks, value);
	default:
		throw ScannerException("Cannot extract parameters from STRUCT value: specified type is not supported, ID: " +
		                       std::to_string(static_cast<uint32_t>(type_id)) +
		                       ", index: " + std::to_string(param_idx));
	}
}

void Types::BindOdbcParam(const QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	switch (param.ParamType()) {
	case ToParamType(LogicalTypeId::SQLNULL):
		TypeSpecific::BindOdbcParam<std::nullptr_t>(ctx, param, param_idx);
		break;
	case ToParamType(LogicalTypeId::BOOLEAN):
		TypeSpecific::BindOdbcParam<bool>(ctx, param, param_idx);
		break;
	case ToParamType(LogicalTypeId::TINYINT):
		TypeSpecific::BindOdbcParam<int8_t>(ctx, param, param_idx);
		break;
	case ToParamType(LogicalTypeId::UTINYINT):
		TypeSpecific::BindOdbcParam<uint8_t>(ctx, param, param_idx);
		break;
	case ToParamType(LogicalTypeId::SMALLINT):
		TypeSpecific::BindOdbcParam<int16_t>(ctx, param, param_idx);
		break;
	case ToParamType(LogicalTypeId::USMALLINT):
		TypeSpecific::BindOdbcParam<uint16_t>(ctx, param, param_idx);
		break;
	case ToParamType(LogicalTypeId::INTEGER):
		TypeSpecific::BindOdbcParam<int32_t>(ctx, param, param_idx);
		break;
	case ToParamType(LogicalTypeId::UINTEGER):
		TypeSpecific::BindOdbcParam<uint32_t>(ctx, param, param_idx);
		break;
	case ToParamType(LogicalTypeId::BIGINT):
		TypeSpecific::BindOdbcParam<int64_t>(ctx, param, param_idx);
		break;
	case ToParamType(LogicalTypeId::UBIGINT):
		TypeSpecific::BindOdbcParam<uint64_t>(ctx, param, param_idx);
		break;
	case ToParamType(LogicalTypeId::FLOAT):
		TypeSpecific::BindOdbcParam<float>(ctx, param, param_idx);
		break;
	case ToParamType(LogicalTypeId::DOUBLE):
		TypeSpecific::BindOdbcParam<double>(ctx, param, param_idx);
		break;
	case ToParamType(LogicalTypeId::DECIMAL):
		TypeSpecific::BindOdbcParam<SQL_NUMERIC_STRUCT>(ctx, param, param_idx);
		break;
	case Params::TYPE_DECIMAL_AS_CHARS:
		TypeSpecific::BindOdbcParam<DecimalChars>(ctx, param, param_idx);
		break;
	case ToParamType(LogicalTypeId::VARCHAR):
		TypeSpecific::BindOdbcParam<std::string>(ctx, param, param_idx);
		break;
	case ToParamType(LogicalTypeId::BLOB):
		TypeSpecific::BindOdbcParam<ScannerBlob>(ctx, param, param_idx);
		break;
	case ToParamType(LogicalTypeId::UUID):
		TypeSpecific::BindOdbcParam<ScannerUuid>(ctx, param, param_idx);
		break;
	case ToParamType(LogicalTypeId::DATE):
		TypeSpecific::BindOdbcParam<DateStruct>(ctx, param, param_idx);
		break;
	case ToParamType(LogicalTypeId::TIME):
	case Params::TYPE_TIME_WITH_NANOS:
		TypeSpecific::BindOdbcParam<TimeStruct>(ctx, param, param_idx);
		break;
	case ToParamType(LogicalTypeId::TIMESTAMP):
	case ToParamType(LogicalTypeId::TIMESTAMP_TZ):
		TypeSpecific::BindOdbcParam<TimestampStruct>(ctx, param, param_idx);
		break;
	default:
		throw ScannerException("Unsupported bind parameter type, ID: " + std::to_string(param.ParamType()));
	}
}

void Types::BindColumn(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx) {
	switch (odbc_type.desc_concise_type) {
	case SQL_BIT:
		TypeSpecific::BindColumn<bool>(ctx, odbc_type, col_idx);
		break;
	case SQL_TINYINT:
		if (odbc_type.is_unsigned) {
			TypeSpecific::BindColumn<uint8_t>(ctx, odbc_type, col_idx);
		} else {
			TypeSpecific::BindColumn<int8_t>(ctx, odbc_type, col_idx);
		}
		break;
	case SQL_SMALLINT:
		if (odbc_type.is_unsigned) {
			TypeSpecific::BindColumn<uint16_t>(ctx, odbc_type, col_idx);
		} else {
			TypeSpecific::BindColumn<int16_t>(ctx, odbc_type, col_idx);
		}
		break;
	case SQL_INTEGER:
		if (odbc_type.is_unsigned) {
			TypeSpecific::BindColumn<uint32_t>(ctx, odbc_type, col_idx);
		} else {
			TypeSpecific::BindColumn<int32_t>(ctx, odbc_type, col_idx);
		}
		break;
	case SQL_BIGINT:
		if (odbc_type.is_unsigned) {
			TypeSpecific::BindColumn<uint64_t>(ctx, odbc_type, col_idx);
		} else {
			TypeSpecific::BindColumn<int64_t>(ctx, odbc_type, col_idx);
		}
		break;
	case SQL_REAL:
		TypeSpecific::BindColumn<float>(ctx, odbc_type, col_idx);
		break;
	case SQL_DOUBLE:
	case SQL_FLOAT:
		TypeSpecific::BindColumn<double>(ctx, odbc_type, col_idx);
		break;
	case SQL_DECIMAL:
	case SQL_NUMERIC:
		TypeSpecific::BindColumn<DecimalValue>(ctx, odbc_type, col_idx);
		break;
	case SQL_GUID:
		TypeSpecific::BindColumn<ScannerUuid>(ctx, odbc_type, col_idx);
		break;
	case SQL_TYPE_DATE:
		TypeSpecific::BindColumn<DateStruct>(ctx, odbc_type, col_idx);
		break;
	case SQL_TYPE_TIME:
	case Types::SQL_SS_TIME2:
		TypeSpecific::BindColumn<TimeStruct>(ctx, odbc_type, col_idx);
		break;
	case SQL_TYPE_TIMESTAMP:
	case SQL_SS_TIMESTAMPOFFSET:
		TypeSpecific::BindColumn<TimestampStruct>(ctx, odbc_type, col_idx);
		break;
		// default: no-op
	}
}

void Types::FetchAndSetResult(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx, Vector &vec,
                              idx_t row_idx) {
	switch (odbc_type.desc_concise_type) {
	case SQL_BIT:
		TypeSpecific::FetchAndSetResult<bool>(ctx, odbc_type, col_idx, vec, row_idx);
		break;
	case SQL_TINYINT:
		if (odbc_type.is_unsigned) {
			TypeSpecific::FetchAndSetResult<uint8_t>(ctx, odbc_type, col_idx, vec, row_idx);
		} else {
			TypeSpecific::FetchAndSetResult<int8_t>(ctx, odbc_type, col_idx, vec, row_idx);
		}
		break;
	case SQL_SMALLINT:
		if (odbc_type.is_unsigned) {
			TypeSpecific::FetchAndSetResult<uint16_t>(ctx, odbc_type, col_idx, vec, row_idx);
		} else {
			TypeSpecific::FetchAndSetResult<int16_t>(ctx, odbc_type, col_idx, vec, row_idx);
		}
		break;
	case SQL_INTEGER:
		if (odbc_type.is_unsigned) {
			TypeSpecific::FetchAndSetResult<uint32_t>(ctx, odbc_type, col_idx, vec, row_idx);
		} else {
			TypeSpecific::FetchAndSetResult<int32_t>(ctx, odbc_type, col_idx, vec, row_idx);
		}
		break;
	case SQL_BIGINT:
		if (odbc_type.is_unsigned) {
			TypeSpecific::FetchAndSetResult<uint64_t>(ctx, odbc_type, col_idx, vec, row_idx);
		} else {
			TypeSpecific::FetchAndSetResult<int64_t>(ctx, odbc_type, col_idx, vec, row_idx);
		}
		break;
	case SQL_REAL:
		TypeSpecific::FetchAndSetResult<float>(ctx, odbc_type, col_idx, vec, row_idx);
		break;
	case SQL_DOUBLE:
	case SQL_FLOAT:
		TypeSpecific::FetchAndSetResult<double>(ctx, odbc_type, col_idx, vec, row_idx);
		break;
	case SQL_DECIMAL:
	case SQL_NUMERIC:
		TypeSpecific::FetchAndSetResult<DecimalValue>(ctx, odbc_type, col_idx, vec, row_idx);
		break;
	case SQL_CHAR:
	case SQL_VARCHAR:
	case SQL_LONGVARCHAR:
	case SQL_WCHAR:
	case SQL_WVARCHAR:
	case SQL_WLONGVARCHAR:
		TypeSpecific::FetchAndSetResult<std::string>(ctx, odbc_type, col_idx, vec, row_idx);
		break;
	case SQL_BINARY:
	case SQL_VARBINARY:
	case SQL_LONGVARBINARY:
		TypeSpecific::FetchAndSetResult<ScannerBlob>(ctx, odbc_type, col_idx, vec, row_idx);
		break;
	case SQL_GUID:
		TypeSpecific::FetchAndSetResult<ScannerUuid>(ctx, odbc_type, col_idx, vec, row_idx);
		break;
	case SQL_TYPE_DATE:
		TypeSpecific::FetchAndSetResult<DateStruct>(ctx, odbc_type, col_idx, vec, row_idx);
		break;
	case SQL_TYPE_TIME:
	case Types::SQL_SS_TIME2:
		TypeSpecific::FetchAndSetResult<TimeStruct>(ctx, odbc_type, col_idx, vec, row_idx);
		break;
	case SQL_TYPE_TIMESTAMP:
	case SQL_SS_TIMESTAMPOFFSET:
		TypeSpecific::FetchAndSetResult<TimestampStruct>(ctx, odbc_type, col_idx, vec, row_idx);
		break;
	default:
		throw ScannerException("Unsupported ODBC fetch type: " + odbc_type.ToString() + ", query: '" + ctx.query +
		                       "', column idx: " + std::to_string(col_idx));
	}
}

void Types::CoalesceParameterType(const QueryContext &ctx, ScannerValue &param) {
	if (ctx.quirks.integral_params_as_decimals && !ctx.quirks.decimal_params_as_chars) {
		param.TransformIntegralToDecimal();
	}
}

void Types::CoalesceColumnType(const QueryContext &ctx, ResultColumn &column) {
	(void)ctx;
	OdbcType &odbc_type = column.odbc_type;
	if (odbc_type.desc_concise_type == Types::SQL_DB2_BLOB && odbc_type.desc_type_name == Types::SQL_BLOB_TYPE_NAME) {
		odbc_type.desc_type = SQL_LONGVARBINARY;
		odbc_type.desc_concise_type = SQL_LONGVARBINARY;
	} else if (odbc_type.desc_concise_type == Types::SQL_DB2_CLOB &&
	           odbc_type.desc_type_name == Types::SQL_CLOB_TYPE_NAME) {
		odbc_type.desc_type = SQL_LONGVARCHAR;
		odbc_type.desc_concise_type = SQL_LONGVARCHAR;
	} else if (odbc_type.desc_concise_type == Types::SQL_DB2_DBCLOB &&
	           odbc_type.desc_type_name == Types::SQL_DB2_DBCLOB_TYPE_NAME) {
		odbc_type.desc_type = SQL_WLONGVARCHAR;
		odbc_type.desc_concise_type = SQL_WLONGVARCHAR;
	} else if (ctx.quirks.timestamp_columns_with_typename_date_as_date && odbc_type.desc_type == SQL_DATE &&
	           odbc_type.desc_concise_type == SQL_TYPE_TIMESTAMP &&
	           odbc_type.desc_type_name == Types::SQL_DATE_TYPE_NAME) {
		odbc_type.desc_type = SQL_TYPE_DATE;
		odbc_type.desc_concise_type = SQL_TYPE_DATE;
	}
}

LogicalTypeId Types::ResolveColumnType(const QueryContext &ctx, ResultColumn &column) {
	OdbcType &odbc_type = column.odbc_type;
	switch (column.odbc_type.desc_concise_type) {
	case SQL_BIT:
		return TypeSpecific::ResolveColumnType<bool>(ctx, column);
	case SQL_TINYINT:
		if (odbc_type.is_unsigned) {
			return TypeSpecific::ResolveColumnType<uint8_t>(ctx, column);
		} else {
			return TypeSpecific::ResolveColumnType<int8_t>(ctx, column);
		}
	case SQL_SMALLINT:
		if (odbc_type.is_unsigned) {
			return TypeSpecific::ResolveColumnType<uint16_t>(ctx, column);
		} else {
			return TypeSpecific::ResolveColumnType<int16_t>(ctx, column);
		}
	case SQL_INTEGER:
		if (odbc_type.is_unsigned) {
			return TypeSpecific::ResolveColumnType<uint32_t>(ctx, column);
		} else {
			return TypeSpecific::ResolveColumnType<int32_t>(ctx, column);
		}
	case SQL_BIGINT:
		if (odbc_type.is_unsigned) {
			return TypeSpecific::ResolveColumnType<uint64_t>(ctx, column);
		} else {
			return TypeSpecific::ResolveColumnType<int64_t>(ctx, column);
		}
	case SQL_REAL:
		return TypeSpecific::ResolveColumnType<float>(ctx, column);
	case SQL_DOUBLE:
	case SQL_FLOAT:
		return TypeSpecific::ResolveColumnType<double>(ctx, column);
		break;
	case SQL_DECIMAL:
	case SQL_NUMERIC:
		return TypeSpecific::ResolveColumnType<DecimalValue>(ctx, column);
	case SQL_CHAR:
	case SQL_VARCHAR:
	case SQL_LONGVARCHAR:
	case SQL_WCHAR:
	case SQL_WVARCHAR:
	case SQL_WLONGVARCHAR:
		return TypeSpecific::ResolveColumnType<std::string>(ctx, column);
	case SQL_BINARY:
	case SQL_VARBINARY:
	case SQL_LONGVARBINARY:
		return TypeSpecific::ResolveColumnType<ScannerBlob>(ctx, column);
	case SQL_GUID:
		return TypeSpecific::ResolveColumnType<ScannerUuid>(ctx, column);
	case SQL_TYPE_DATE:
		return TypeSpecific::ResolveColumnType<DateStruct>(ctx, column);
	case SQL_TYPE_TIME:
	case Types::SQL_SS_TIME2:
		return TypeSpecific::ResolveColumnType<TimeStruct>(ctx, column);
	case SQL_TYPE_TIMESTAMP:
	case Types::SQL_SS_TIMESTAMPOFFSET:
		return TypeSpecific::ResolveColumnType<TimestampStruct>(ctx, column);
	default:
		throw ScannerException("Unsupported ODBC column type: " + odbc_type.ToString() + ", query: '" + ctx.query +
		                       "', column name: '" + column.name + "'");
	}
}

std::string Types::ToString(LogicalTypeId type_id) {
	switch (type_id) {
	case LogicalTypeId::INVALID:
		return "DUCKDB_TYPE_INVALID";
	case LogicalTypeId::BOOLEAN:
		return "DUCKDB_TYPE_BOOLEAN";
	case LogicalTypeId::TINYINT:
		return "DUCKDB_TYPE_TINYINT";
	case LogicalTypeId::SMALLINT:
		return "DUCKDB_TYPE_SMALLINT";
	case LogicalTypeId::INTEGER:
		return "DUCKDB_TYPE_INTEGER";
	case LogicalTypeId::BIGINT:
		return "DUCKDB_TYPE_BIGINT";
	case LogicalTypeId::UTINYINT:
		return "DUCKDB_TYPE_UTINYINT";
	case LogicalTypeId::USMALLINT:
		return "DUCKDB_TYPE_USMALLINT";
	case LogicalTypeId::UINTEGER:
		return "DUCKDB_TYPE_UINTEGER";
	case LogicalTypeId::UBIGINT:
		return "DUCKDB_TYPE_UBIGINT";
	case LogicalTypeId::FLOAT:
		return "DUCKDB_TYPE_FLOAT";
	case LogicalTypeId::DOUBLE:
		return "DUCKDB_TYPE_DOUBLE";
	case LogicalTypeId::TIMESTAMP:
		return "DUCKDB_TYPE_TIMESTAMP";
	case LogicalTypeId::DATE:
		return "DUCKDB_TYPE_DATE";
	case LogicalTypeId::TIME:
		return "DUCKDB_TYPE_TIME";
	case LogicalTypeId::INTERVAL:
		return "DUCKDB_TYPE_INTERVAL";
	case LogicalTypeId::HUGEINT:
		return "DUCKDB_TYPE_HUGEINT";
	case LogicalTypeId::UHUGEINT:
		return "DUCKDB_TYPE_UHUGEINT";
	case LogicalTypeId::VARCHAR:
		return "DUCKDB_TYPE_VARCHAR";
	case LogicalTypeId::BLOB:
		return "DUCKDB_TYPE_BLOB";
	case LogicalTypeId::DECIMAL:
		return "DUCKDB_TYPE_DECIMAL";
	case LogicalTypeId::TIMESTAMP_SEC:
		return "DUCKDB_TYPE_TIMESTAMP_S";
	case LogicalTypeId::TIMESTAMP_MS:
		return "DUCKDB_TYPE_TIMESTAMP_MS";
	case LogicalTypeId::TIMESTAMP_NS:
		return "DUCKDB_TYPE_TIMESTAMP_NS";
	case LogicalTypeId::ENUM:
		return "DUCKDB_TYPE_ENUM";
	case LogicalTypeId::LIST:
		return "DUCKDB_TYPE_LIST";
	case LogicalTypeId::STRUCT:
		return "DUCKDB_TYPE_STRUCT";
	case LogicalTypeId::MAP:
		return "DUCKDB_TYPE_MAP";
	case LogicalTypeId::ARRAY:
		return "DUCKDB_TYPE_ARRAY";
	case LogicalTypeId::UUID:
		return "DUCKDB_TYPE_UUID";
	case LogicalTypeId::UNION:
		return "DUCKDB_TYPE_UNION";
	case LogicalTypeId::BIT:
		return "DUCKDB_TYPE_BIT";
	case LogicalTypeId::TIME_TZ:
		return "DUCKDB_TYPE_TIME_TZ";
	case LogicalTypeId::TIMESTAMP_TZ:
		return "DUCKDB_TYPE_TIMESTAMP_TZ";
	case LogicalTypeId::ANY:
		return "DUCKDB_TYPE_ANY";
	case LogicalTypeId::BIGNUM:
		return "DUCKDB_TYPE_BIGNUM";
	case LogicalTypeId::SQLNULL:
		return "DUCKDB_TYPE_SQLNULL";
	default:
		return UNKNOWN_DUCKDB_TYPE_NAME;
	}
}

LogicalTypeId Types::FromString(const std::string &type_name) {
	if (type_name == "DUCKDB_TYPE_INVALID") {
		return LogicalTypeId::INVALID;
	} else if (type_name == "DUCKDB_TYPE_BOOLEAN") {
		return LogicalTypeId::BOOLEAN;
	} else if (type_name == "DUCKDB_TYPE_TINYINT") {
		return LogicalTypeId::TINYINT;
	} else if (type_name == "DUCKDB_TYPE_SMALLINT") {
		return LogicalTypeId::SMALLINT;
	} else if (type_name == "DUCKDB_TYPE_INTEGER") {
		return LogicalTypeId::INTEGER;
	} else if (type_name == "DUCKDB_TYPE_BIGINT") {
		return LogicalTypeId::BIGINT;
	} else if (type_name == "DUCKDB_TYPE_UTINYINT") {
		return LogicalTypeId::UTINYINT;
	} else if (type_name == "DUCKDB_TYPE_USMALLINT") {
		return LogicalTypeId::USMALLINT;
	} else if (type_name == "DUCKDB_TYPE_UINTEGER") {
		return LogicalTypeId::UINTEGER;
	} else if (type_name == "DUCKDB_TYPE_UBIGINT") {
		return LogicalTypeId::UBIGINT;
	} else if (type_name == "DUCKDB_TYPE_FLOAT") {
		return LogicalTypeId::FLOAT;
	} else if (type_name == "DUCKDB_TYPE_DOUBLE") {
		return LogicalTypeId::DOUBLE;
	} else if (type_name == "DUCKDB_TYPE_TIMESTAMP") {
		return LogicalTypeId::TIMESTAMP;
	} else if (type_name == "DUCKDB_TYPE_DATE") {
		return LogicalTypeId::DATE;
	} else if (type_name == "DUCKDB_TYPE_TIME") {
		return LogicalTypeId::TIME;
	} else if (type_name == "DUCKDB_TYPE_INTERVAL") {
		return LogicalTypeId::INTERVAL;
	} else if (type_name == "DUCKDB_TYPE_HUGEINT") {
		return LogicalTypeId::HUGEINT;
	} else if (type_name == "DUCKDB_TYPE_UHUGEINT") {
		return LogicalTypeId::UHUGEINT;
	} else if (type_name == "DUCKDB_TYPE_VARCHAR") {
		return LogicalTypeId::VARCHAR;
	} else if (type_name == "DUCKDB_TYPE_BLOB") {
		return LogicalTypeId::BLOB;
	} else if (type_name == "DUCKDB_TYPE_DECIMAL") {
		return LogicalTypeId::DECIMAL;
	} else if (type_name == "DUCKDB_TYPE_TIMESTAMP_S") {
		return LogicalTypeId::TIMESTAMP_SEC;
	} else if (type_name == "DUCKDB_TYPE_TIMESTAMP_MS") {
		return LogicalTypeId::TIMESTAMP_MS;
	} else if (type_name == "DUCKDB_TYPE_TIMESTAMP_NS") {
		return LogicalTypeId::TIMESTAMP_NS;
	} else if (type_name == "DUCKDB_TYPE_ENUM") {
		return LogicalTypeId::ENUM;
	} else if (type_name == "DUCKDB_TYPE_LIST") {
		return LogicalTypeId::LIST;
	} else if (type_name == "DUCKDB_TYPE_STRUCT") {
		return LogicalTypeId::STRUCT;
	} else if (type_name == "DUCKDB_TYPE_MAP") {
		return LogicalTypeId::MAP;
	} else if (type_name == "DUCKDB_TYPE_ARRAY") {
		return LogicalTypeId::ARRAY;
	} else if (type_name == "DUCKDB_TYPE_UUID") {
		return LogicalTypeId::UUID;
	} else if (type_name == "DUCKDB_TYPE_UNION") {
		return LogicalTypeId::UNION;
	} else if (type_name == "DUCKDB_TYPE_BIT") {
		return LogicalTypeId::BIT;
	} else if (type_name == "DUCKDB_TYPE_TIME_TZ") {
		return LogicalTypeId::TIME_TZ;
	} else if (type_name == "DUCKDB_TYPE_TIMESTAMP_TZ") {
		return LogicalTypeId::TIMESTAMP_TZ;
	} else if (type_name == "DUCKDB_TYPE_ANY") {
		return LogicalTypeId::ANY;
	} else if (type_name == "DUCKDB_TYPE_BIGNUM") {
		return LogicalTypeId::BIGNUM;
	} else if (type_name == "DUCKDB_TYPE_SQLNULL") {
		return LogicalTypeId::SQLNULL;
	} else {
		throw ScannerException("Unknown DuckDB type name specified: '" + type_name + "'");
	}
}

} // namespace odbcscanner
