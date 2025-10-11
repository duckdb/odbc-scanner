#include "types.hpp"

#include <cstdint>
#include <vector>

#include "columns.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"
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

ScannerParam Types::ExtractNotNullParamOfType(DbmsQuirks &quirks, duckdb_type type_id, duckdb_vector vec,
                                              idx_t param_idx) {
	switch (type_id) {
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
	case DUCKDB_TYPE_DATE:
		return TypeSpecific::ExtractNotNullParam<duckdb_date_struct>(quirks, vec);
	case DUCKDB_TYPE_TIME:
		return TypeSpecific::ExtractNotNullParam<duckdb_time_struct>(quirks, vec);
	case DUCKDB_TYPE_TIMESTAMP:
		return TypeSpecific::ExtractNotNullParam<duckdb_timestamp_struct>(quirks, vec);
	default:
		throw ScannerException("Cannot extract parameters from STRUCT: specified type is not supported, id: " +
		                       std::to_string(type_id) + ", index: " + std::to_string(param_idx));
	}
}

ScannerParam Types::ExtractNotNullParamFromValue(DbmsQuirks &quirks, duckdb_value value, idx_t param_idx) {
	duckdb_logical_type ltype = duckdb_get_value_type(value);
	auto type_id = duckdb_get_type_id(ltype);
	switch (type_id) {
	case DUCKDB_TYPE_TINYINT:
		return ScannerParam(duckdb_get_int8(value));
	case DUCKDB_TYPE_UTINYINT:
		return ScannerParam(duckdb_get_uint8(value));
	case DUCKDB_TYPE_SMALLINT:
		return ScannerParam(duckdb_get_int16(value));
	case DUCKDB_TYPE_USMALLINT:
		return ScannerParam(duckdb_get_uint16(value));
	case DUCKDB_TYPE_INTEGER:
		return ScannerParam(duckdb_get_int32(value));
	case DUCKDB_TYPE_UINTEGER:
		return ScannerParam(duckdb_get_uint32(value));
	case DUCKDB_TYPE_BIGINT:
		return ScannerParam(duckdb_get_int64(value));
	case DUCKDB_TYPE_UBIGINT:
		return ScannerParam(duckdb_get_uint64(value));
	case DUCKDB_TYPE_FLOAT:
		return ScannerParam(duckdb_get_float(value));
	case DUCKDB_TYPE_DOUBLE:
		return ScannerParam(duckdb_get_double(value));
	case DUCKDB_TYPE_DECIMAL:
		return ScannerParam(duckdb_get_decimal(value), quirks.decimal_params_as_chars);
	case DUCKDB_TYPE_VARCHAR:
		return ScannerParam(duckdb_get_varchar(value));
	case DUCKDB_TYPE_DATE:
		return ScannerParam(duckdb_from_date(duckdb_get_date(value)));
	case DUCKDB_TYPE_TIME:
		return ScannerParam(duckdb_from_time(duckdb_get_time(value)));
	case DUCKDB_TYPE_TIMESTAMP:
		return ScannerParam(duckdb_from_timestamp(duckdb_get_timestamp(value)));
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
	case DUCKDB_TYPE_DATE:
		TypeSpecific::BindOdbcParam<duckdb_date_struct>(ctx, param, param_idx);
		break;
	case DUCKDB_TYPE_TIME:
		TypeSpecific::BindOdbcParam<duckdb_time_struct>(ctx, param, param_idx);
		break;
	case DUCKDB_TYPE_TIMESTAMP:
		TypeSpecific::BindOdbcParam<duckdb_timestamp_struct>(ctx, param, param_idx);
		break;
	default:
		throw ScannerException("Unsupported parameter type, ID: " + std::to_string(param.ParamType()));
	}
}

void Types::FetchAndSetResultOfType(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx, duckdb_vector vec,
                                    idx_t row_idx) {
	switch (odbc_type.desc_concise_type) {
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
		TypeSpecific::FetchAndSetResult<double>(ctx, odbc_type, col_idx, vec, row_idx);
		break;
	case SQL_FLOAT:
		if (ctx.quirks.float_width_bytes <= sizeof(float)) {
			TypeSpecific::FetchAndSetResult<float>(ctx, odbc_type, col_idx, vec, row_idx);
		} else {
			TypeSpecific::FetchAndSetResult<double>(ctx, odbc_type, col_idx, vec, row_idx);
		}
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
	case SQL_TYPE_DATE:
		TypeSpecific::FetchAndSetResult<duckdb_date_struct>(ctx, odbc_type, col_idx, vec, row_idx);
		break;
	case SQL_TYPE_TIME:
		TypeSpecific::FetchAndSetResult<duckdb_time_struct>(ctx, odbc_type, col_idx, vec, row_idx);
		break;
	case SQL_TYPE_TIMESTAMP:
		TypeSpecific::FetchAndSetResult<duckdb_timestamp_struct>(ctx, odbc_type, col_idx, vec, row_idx);
		break;
	case Types::SQL_SS_TIME2:
		if (odbc_type.decimal_precision <= 6) {
			TypeSpecific::FetchAndSetResult<duckdb_time_struct>(ctx, odbc_type, col_idx, vec, row_idx);
			break;
		}
		// fall through
	default:
		throw ScannerException("Unsupported ODBC fetch type: " + odbc_type.ToString() + ", query: '" + ctx.query +
		                       "', column idx: " + std::to_string(col_idx));
	}
}

duckdb_type Types::ResolveColumnType(QueryContext &ctx, ResultColumn &column) {
	switch (column.odbc_type.desc_concise_type) {
	case SQL_TINYINT:
		if (column.odbc_type.is_unsigned) {
			return TypeSpecific::ResolveColumnType<uint8_t>(ctx, column);
		} else {
			return TypeSpecific::ResolveColumnType<int8_t>(ctx, column);
		}
	case SQL_SMALLINT:
		if (column.odbc_type.is_unsigned) {
			return TypeSpecific::ResolveColumnType<uint16_t>(ctx, column);
		} else {
			return TypeSpecific::ResolveColumnType<int16_t>(ctx, column);
		}
	case SQL_INTEGER:
		if (column.odbc_type.is_unsigned) {
			return TypeSpecific::ResolveColumnType<uint32_t>(ctx, column);
		} else {
			return TypeSpecific::ResolveColumnType<int32_t>(ctx, column);
		}
	case SQL_BIGINT:
		if (column.odbc_type.is_unsigned) {
			return TypeSpecific::ResolveColumnType<uint64_t>(ctx, column);
		} else {
			return TypeSpecific::ResolveColumnType<int64_t>(ctx, column);
		}
	case SQL_REAL:
		return TypeSpecific::ResolveColumnType<float>(ctx, column);
	case SQL_DOUBLE:
		return TypeSpecific::ResolveColumnType<double>(ctx, column);
	case SQL_FLOAT:
		if (ctx.quirks.float_width_bytes <= sizeof(float)) {
			return TypeSpecific::ResolveColumnType<float>(ctx, column);
		} else {
			return TypeSpecific::ResolveColumnType<double>(ctx, column);
		}
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
	case SQL_TYPE_DATE:
		return TypeSpecific::ResolveColumnType<duckdb_date_struct>(ctx, column);
	case SQL_TYPE_TIME:
	case Types::SQL_SS_TIME2:
		return TypeSpecific::ResolveColumnType<duckdb_time_struct>(ctx, column);
	case SQL_TYPE_TIMESTAMP:
		return TypeSpecific::ResolveColumnType<duckdb_timestamp_struct>(ctx, column);
	default:
		throw ScannerException("Unsupported ODBC column type: " + column.odbc_type.ToString() + ", query: '" +
		                       ctx.query + "', column name: '" + column.name + "'");
	}
}

} // namespace odbcscanner
