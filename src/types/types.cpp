#include "types.hpp"

#include <cstdint>
#include <vector>

#include "columns.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"
#include "temporal.hpp"
#include "widechar.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

std::string OdbcType::ToString() {
	return "code: " + std::to_string(desc_type) + ", concise type: " + std::to_string(desc_concise_type) +
	       ", type name: '" + desc_type_name + "', unsigned: " + std::to_string(is_unsigned) +
	       ", precision: " + std::to_string(decimal_precision) + ", scale: " + std::to_string(decimal_scale);
}

bool OdbcType::Equals(OdbcType &other) {
	return desc_type == other.desc_type && desc_concise_type == other.desc_concise_type &&
	       desc_type_name == other.desc_type_name && is_unsigned == other.is_unsigned &&
	       decimal_precision == other.decimal_precision && decimal_scale == other.decimal_scale;
}

const std::string Types::MSSQL_DATETIME2_TYPE_NAME = "datetime2";
const std::string Types::SQL_DATE_TYPE_NAME = "DATE";
const std::string Types::SQL_BLOB_TYPE_NAME = "BLOB";
const std::string Types::SQL_CLOB_TYPE_NAME = "CLOB";
const std::string Types::SQL_DB2_DBCLOB_TYPE_NAME = "DBCLOB";

ScannerParam Types::ExtractNotNullParam(DbmsQuirks &quirks, duckdb_type type_id, duckdb_vector vec, idx_t param_idx) {
	switch (type_id) {
	case DUCKDB_TYPE_BOOLEAN:
		return TypeSpecific::ExtractNotNullParam<bool>(quirks, vec);
	case DUCKDB_TYPE_TINYINT:
		return TypeSpecific::ExtractNotNullParam<int8_t>(quirks, vec);
	case DUCKDB_TYPE_UTINYINT:
		return TypeSpecific::ExtractNotNullParam<uint8_t>(quirks, vec);
	case DUCKDB_TYPE_SMALLINT:
		return TypeSpecific::ExtractNotNullParam<int16_t>(quirks, vec);
	case DUCKDB_TYPE_USMALLINT:
		return TypeSpecific::ExtractNotNullParam<uint16_t>(quirks, vec);
	case DUCKDB_TYPE_INTEGER:
		return TypeSpecific::ExtractNotNullParam<int32_t>(quirks, vec);
	case DUCKDB_TYPE_UINTEGER:
		return TypeSpecific::ExtractNotNullParam<uint32_t>(quirks, vec);
	case DUCKDB_TYPE_BIGINT:
		return TypeSpecific::ExtractNotNullParam<int64_t>(quirks, vec);
	case DUCKDB_TYPE_UBIGINT:
		return TypeSpecific::ExtractNotNullParam<uint64_t>(quirks, vec);
	case DUCKDB_TYPE_FLOAT:
		return TypeSpecific::ExtractNotNullParam<float>(quirks, vec);
	case DUCKDB_TYPE_DOUBLE:
		return TypeSpecific::ExtractNotNullParam<double>(quirks, vec);
	case DUCKDB_TYPE_DECIMAL:
		return TypeSpecific::ExtractNotNullParam<duckdb_decimal>(quirks, vec);
	case DUCKDB_TYPE_VARCHAR:
		return TypeSpecific::ExtractNotNullParam<std::string>(quirks, vec);
	case DUCKDB_TYPE_BLOB:
		return TypeSpecific::ExtractNotNullParam<duckdb_blob>(quirks, vec);
	case DUCKDB_TYPE_UUID:
		return TypeSpecific::ExtractNotNullParam<OdbcUuid>(quirks, vec);
	case DUCKDB_TYPE_DATE:
		return TypeSpecific::ExtractNotNullParam<duckdb_date_struct>(quirks, vec);
	case DUCKDB_TYPE_TIME:
		return TypeSpecific::ExtractNotNullParam<duckdb_time_struct>(quirks, vec);
	case DUCKDB_TYPE_TIMESTAMP:
	case DUCKDB_TYPE_TIMESTAMP_TZ:
		return TypeSpecific::ExtractNotNullParam<duckdb_timestamp_struct>(quirks, vec);
	case DUCKDB_TYPE_TIMESTAMP_NS:
		return TypeSpecific::ExtractNotNullParam<TimestampNsStruct>(quirks, vec);
	default:
		throw ScannerException("Cannot extract parameters from STRUCT: specified type is not supported, id: " +
		                       std::to_string(type_id) + ", index: " + std::to_string(param_idx));
	}
}

ScannerParam Types::ExtractNotNullParam(DbmsQuirks &quirks, duckdb_value value, idx_t param_idx) {
	duckdb_logical_type ltype = duckdb_get_value_type(value);
	auto type_id = duckdb_get_type_id(ltype);
	switch (type_id) {
	case DUCKDB_TYPE_BOOLEAN:
		return TypeSpecific::ExtractNotNullParam<bool>(quirks, value);
	case DUCKDB_TYPE_TINYINT:
		return TypeSpecific::ExtractNotNullParam<int8_t>(quirks, value);
	case DUCKDB_TYPE_UTINYINT:
		return TypeSpecific::ExtractNotNullParam<uint8_t>(quirks, value);
	case DUCKDB_TYPE_SMALLINT:
		return TypeSpecific::ExtractNotNullParam<int16_t>(quirks, value);
	case DUCKDB_TYPE_USMALLINT:
		return TypeSpecific::ExtractNotNullParam<uint16_t>(quirks, value);
	case DUCKDB_TYPE_INTEGER:
		return TypeSpecific::ExtractNotNullParam<int32_t>(quirks, value);
	case DUCKDB_TYPE_UINTEGER:
		return TypeSpecific::ExtractNotNullParam<uint32_t>(quirks, value);
	case DUCKDB_TYPE_BIGINT:
		return TypeSpecific::ExtractNotNullParam<int64_t>(quirks, value);
	case DUCKDB_TYPE_UBIGINT:
		return TypeSpecific::ExtractNotNullParam<uint64_t>(quirks, value);
	case DUCKDB_TYPE_FLOAT:
		return TypeSpecific::ExtractNotNullParam<float>(quirks, value);
	case DUCKDB_TYPE_DOUBLE:
		return TypeSpecific::ExtractNotNullParam<double>(quirks, value);
	case DUCKDB_TYPE_DECIMAL:
		return TypeSpecific::ExtractNotNullParam<duckdb_decimal>(quirks, value);
	case DUCKDB_TYPE_VARCHAR:
		return TypeSpecific::ExtractNotNullParam<std::string>(quirks, value);
	case DUCKDB_TYPE_BLOB:
		return TypeSpecific::ExtractNotNullParam<duckdb_blob>(quirks, value);
	case DUCKDB_TYPE_UUID:
		return TypeSpecific::ExtractNotNullParam<OdbcUuid>(quirks, value);
	case DUCKDB_TYPE_DATE:
		return TypeSpecific::ExtractNotNullParam<duckdb_date_struct>(quirks, value);
	case DUCKDB_TYPE_TIME:
		return TypeSpecific::ExtractNotNullParam<duckdb_time_struct>(quirks, value);
	case DUCKDB_TYPE_TIMESTAMP:
	case DUCKDB_TYPE_TIMESTAMP_TZ:
		return TypeSpecific::ExtractNotNullParam<duckdb_timestamp_struct>(quirks, value);
	case DUCKDB_TYPE_TIMESTAMP_NS:
		return TypeSpecific::ExtractNotNullParam<TimestampNsStruct>(quirks, value);
	default:
		throw ScannerException("Cannot extract parameters from STRUCT value: specified type is not supported, ID: " +
		                       std::to_string(type_id) + ", index: " + std::to_string(param_idx));
	}
}

void Types::BindOdbcParam(QueryContext &ctx, ScannerParam &param, SQLSMALLINT param_idx) {
	switch (param.ParamType()) {
	case DUCKDB_TYPE_SQLNULL:
		TypeSpecific::BindOdbcParam<std::nullptr_t>(ctx, param, param_idx);
		break;
	case DUCKDB_TYPE_BOOLEAN:
		TypeSpecific::BindOdbcParam<bool>(ctx, param, param_idx);
		break;
	case DUCKDB_TYPE_TINYINT:
		TypeSpecific::BindOdbcParam<int8_t>(ctx, param, param_idx);
		break;
	case DUCKDB_TYPE_UTINYINT:
		TypeSpecific::BindOdbcParam<uint8_t>(ctx, param, param_idx);
		break;
	case DUCKDB_TYPE_SMALLINT:
		TypeSpecific::BindOdbcParam<int16_t>(ctx, param, param_idx);
		break;
	case DUCKDB_TYPE_USMALLINT:
		TypeSpecific::BindOdbcParam<uint16_t>(ctx, param, param_idx);
		break;
	case DUCKDB_TYPE_INTEGER:
		TypeSpecific::BindOdbcParam<int32_t>(ctx, param, param_idx);
		break;
	case DUCKDB_TYPE_UINTEGER:
		TypeSpecific::BindOdbcParam<uint32_t>(ctx, param, param_idx);
		break;
	case DUCKDB_TYPE_BIGINT:
		TypeSpecific::BindOdbcParam<int64_t>(ctx, param, param_idx);
		break;
	case DUCKDB_TYPE_UBIGINT:
		TypeSpecific::BindOdbcParam<uint64_t>(ctx, param, param_idx);
		break;
	case DUCKDB_TYPE_FLOAT:
		TypeSpecific::BindOdbcParam<float>(ctx, param, param_idx);
		break;
	case DUCKDB_TYPE_DOUBLE:
		TypeSpecific::BindOdbcParam<double>(ctx, param, param_idx);
		break;
	case DUCKDB_TYPE_DECIMAL:
		TypeSpecific::BindOdbcParam<SQL_NUMERIC_STRUCT>(ctx, param, param_idx);
		break;
	case Params::TYPE_DECIMAL_AS_CHARS:
		TypeSpecific::BindOdbcParam<DecimalChars>(ctx, param, param_idx);
		break;
	case DUCKDB_TYPE_VARCHAR:
		TypeSpecific::BindOdbcParam<std::string>(ctx, param, param_idx);
		break;
	case DUCKDB_TYPE_BLOB:
		TypeSpecific::BindOdbcParam<duckdb_blob>(ctx, param, param_idx);
		break;
	case DUCKDB_TYPE_UUID:
		TypeSpecific::BindOdbcParam<OdbcUuid>(ctx, param, param_idx);
		break;
	case DUCKDB_TYPE_DATE:
		TypeSpecific::BindOdbcParam<duckdb_date_struct>(ctx, param, param_idx);
		break;
	case DUCKDB_TYPE_TIME:
	case Params::TYPE_TIME_WITH_NANOS:
		TypeSpecific::BindOdbcParam<duckdb_time_struct>(ctx, param, param_idx);
		break;
	case DUCKDB_TYPE_TIMESTAMP:
	case DUCKDB_TYPE_TIMESTAMP_TZ:
		TypeSpecific::BindOdbcParam<duckdb_timestamp_struct>(ctx, param, param_idx);
		break;
	default:
		throw ScannerException("Unsupported bind parameter type, ID: " + std::to_string(param.ParamType()));
	}
}

void Types::SetColumnDescriptors(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx) {
	switch (odbc_type.desc_concise_type) {
	case SQL_DECIMAL:
	case SQL_NUMERIC:
		TypeSpecific::SetColumnDescriptors<duckdb_decimal>(ctx, odbc_type, col_idx);
		break;
		// default: no-op for all types except NUMERIC
	}
}

void Types::FetchAndSetResult(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx, duckdb_vector vec,
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
		TypeSpecific::FetchAndSetResult<duckdb_decimal>(ctx, odbc_type, col_idx, vec, row_idx);
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
		TypeSpecific::FetchAndSetResult<duckdb_blob>(ctx, odbc_type, col_idx, vec, row_idx);
		break;
	case SQL_GUID:
		TypeSpecific::FetchAndSetResult<OdbcUuid>(ctx, odbc_type, col_idx, vec, row_idx);
		break;
	case SQL_TYPE_DATE:
		TypeSpecific::FetchAndSetResult<duckdb_date_struct>(ctx, odbc_type, col_idx, vec, row_idx);
		break;
	case SQL_TYPE_TIME:
		TypeSpecific::FetchAndSetResult<duckdb_time_struct>(ctx, odbc_type, col_idx, vec, row_idx);
		break;
	case SQL_TYPE_TIMESTAMP:
	case SQL_SS_TIMESTAMPOFFSET:
		TypeSpecific::FetchAndSetResult<duckdb_timestamp_struct>(ctx, odbc_type, col_idx, vec, row_idx);
		break;
	case Types::SQL_SS_TIME2:
		TypeSpecific::FetchAndSetResult<duckdb_time_struct>(ctx, odbc_type, col_idx, vec, row_idx);
		break;
	default:
		throw ScannerException("Unsupported ODBC fetch type: " + odbc_type.ToString() + ", query: '" + ctx.query +
		                       "', column idx: " + std::to_string(col_idx));
	}
}

void Types::CoalesceColumnType(QueryContext &ctx, ResultColumn &column) {
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
	}
}

duckdb_type Types::ResolveColumnType(QueryContext &ctx, ResultColumn &column) {
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
		return TypeSpecific::ResolveColumnType<duckdb_decimal>(ctx, column);
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
		return TypeSpecific::ResolveColumnType<duckdb_blob>(ctx, column);
	case SQL_GUID:
		return TypeSpecific::ResolveColumnType<OdbcUuid>(ctx, column);
	case SQL_TYPE_DATE:
		return TypeSpecific::ResolveColumnType<duckdb_date_struct>(ctx, column);
	case SQL_TYPE_TIME:
	case Types::SQL_SS_TIME2:
		return TypeSpecific::ResolveColumnType<duckdb_time_struct>(ctx, column);
	case SQL_TYPE_TIMESTAMP:
	case Types::SQL_SS_TIMESTAMPOFFSET:
		return TypeSpecific::ResolveColumnType<duckdb_timestamp_struct>(ctx, column);
	default:
		throw ScannerException("Unsupported ODBC column type: " + odbc_type.ToString() + ", query: '" + ctx.query +
		                       "', column name: '" + column.name + "'");
	}
}

} // namespace odbcscanner
