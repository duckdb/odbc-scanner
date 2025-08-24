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
	return "type: " + std::to_string(desc_type) + ", concise type: " + std::to_string(desc_concise_type) +
	       ", type name: '" + desc_type_name + "'";
}

bool OdbcType::Equals(OdbcType &other) {
	return desc_type == other.desc_type && desc_concise_type == other.desc_concise_type &&
	       desc_type_name == other.desc_type_name;
}

ScannerParam Types::ExtractNotNullParamOfType(duckdb_type type_id, duckdb_vector vec, idx_t param_idx) {
	switch (type_id) {
	case DUCKDB_TYPE_TINYINT:
		return TypeSpecific::ExtractNotNullParam<int8_t>(vec);
	case DUCKDB_TYPE_UTINYINT:
		return TypeSpecific::ExtractNotNullParam<uint8_t>(vec);
	case DUCKDB_TYPE_SMALLINT:
		return TypeSpecific::ExtractNotNullParam<int16_t>(vec);
	case DUCKDB_TYPE_USMALLINT:
		return TypeSpecific::ExtractNotNullParam<uint16_t>(vec);
	case DUCKDB_TYPE_INTEGER:
		return TypeSpecific::ExtractNotNullParam<int32_t>(vec);
	case DUCKDB_TYPE_UINTEGER:
		return TypeSpecific::ExtractNotNullParam<uint32_t>(vec);
	case DUCKDB_TYPE_BIGINT:
		return TypeSpecific::ExtractNotNullParam<int64_t>(vec);
	case DUCKDB_TYPE_UBIGINT:
		return TypeSpecific::ExtractNotNullParam<uint64_t>(vec);
	case DUCKDB_TYPE_VARCHAR:
		return TypeSpecific::ExtractNotNullParam<std::string>(vec);
	default:
		throw ScannerException("Cannot extract parameters from STRUCT: specified type is not supported, id: " +
		                       std::to_string(type_id) + ", index: " + std::to_string(param_idx));
	}
}

ScannerParam Types::ExtractNotNullParamFromValue(duckdb_value value, idx_t param_idx) {
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
	case DUCKDB_TYPE_VARCHAR:
		return ScannerParam(duckdb_get_varchar(value));
	default:
		throw ScannerException("Cannot extract parameters from STRUCT value: specified type is not supported, ID: " +
		                       std::to_string(type_id) + ", index: " + std::to_string(param_idx));
	}
}

void Types::BindOdbcParam(const std::string &query, HSTMT hstmt, ScannerParam &param, SQLSMALLINT param_idx) {
	switch (param.TypeId()) {
	case DUCKDB_TYPE_SQLNULL:
		TypeSpecific::BindOdbcParam<std::nullptr_t>(query, hstmt, param, param_idx);
		break;
	case DUCKDB_TYPE_TINYINT:
		TypeSpecific::BindOdbcParam<int8_t>(query, hstmt, param, param_idx);
		break;
	case DUCKDB_TYPE_UTINYINT:
		TypeSpecific::BindOdbcParam<uint8_t>(query, hstmt, param, param_idx);
		break;
	case DUCKDB_TYPE_SMALLINT:
		TypeSpecific::BindOdbcParam<int16_t>(query, hstmt, param, param_idx);
		break;
	case DUCKDB_TYPE_USMALLINT:
		TypeSpecific::BindOdbcParam<uint16_t>(query, hstmt, param, param_idx);
		break;
	case DUCKDB_TYPE_INTEGER:
		TypeSpecific::BindOdbcParam<int32_t>(query, hstmt, param, param_idx);
		break;
	case DUCKDB_TYPE_UINTEGER:
		TypeSpecific::BindOdbcParam<uint32_t>(query, hstmt, param, param_idx);
		break;
	case DUCKDB_TYPE_BIGINT:
		TypeSpecific::BindOdbcParam<int64_t>(query, hstmt, param, param_idx);
		break;
	case DUCKDB_TYPE_UBIGINT:
		TypeSpecific::BindOdbcParam<uint64_t>(query, hstmt, param, param_idx);
		break;
	case DUCKDB_TYPE_VARCHAR:
		TypeSpecific::BindOdbcParam<std::string>(query, hstmt, param, param_idx);
		break;
	default:
		throw ScannerException("Unsupported parameter type, ID: " + std::to_string(param.TypeId()));
	}
}

void Types::FetchAndSetResultOfType(const OdbcType &odbc_type, const std::string &query, HSTMT hstmt,
                                    SQLSMALLINT col_idx, duckdb_vector vec, idx_t row_idx) {
	switch (odbc_type.desc_concise_type) {
	case SQL_TINYINT:
		if (odbc_type.is_unsigned) {
			TypeSpecific::FetchAndSetResult<uint8_t>(query, hstmt, col_idx, vec, row_idx);
		} else {
			TypeSpecific::FetchAndSetResult<int8_t>(query, hstmt, col_idx, vec, row_idx);
		}
		break;
	case SQL_SMALLINT:
		if (odbc_type.is_unsigned) {
			TypeSpecific::FetchAndSetResult<uint16_t>(query, hstmt, col_idx, vec, row_idx);
		} else {
			TypeSpecific::FetchAndSetResult<int16_t>(query, hstmt, col_idx, vec, row_idx);
		}
		break;
	case SQL_INTEGER:
		if (odbc_type.is_unsigned) {
			TypeSpecific::FetchAndSetResult<uint32_t>(query, hstmt, col_idx, vec, row_idx);
		} else {
			TypeSpecific::FetchAndSetResult<int32_t>(query, hstmt, col_idx, vec, row_idx);
		}
		break;
	case SQL_BIGINT:
		if (odbc_type.is_unsigned) {
			TypeSpecific::FetchAndSetResult<uint64_t>(query, hstmt, col_idx, vec, row_idx);
		} else {
			TypeSpecific::FetchAndSetResult<int64_t>(query, hstmt, col_idx, vec, row_idx);
		}
		break;
	case SQL_VARCHAR:
		TypeSpecific::FetchAndSetResult<std::string>(query, hstmt, col_idx, vec, row_idx);
		break;
	default:
		throw ScannerException("Unsupported ODBC fetch type: " + std::to_string(odbc_type.desc_concise_type) +
		                       ", name: '" + odbc_type.desc_type_name + "'");
	}
}

SQLSMALLINT Types::DuckParamTypeToOdbc(duckdb_type type_id, size_t param_idx) {
	switch (type_id) {
	case DUCKDB_TYPE_SQLNULL:
		return SQL_TYPE_NULL;
	case DUCKDB_TYPE_TINYINT:
	case DUCKDB_TYPE_UTINYINT:
		return SQL_TINYINT;
	case DUCKDB_TYPE_SMALLINT:
	case DUCKDB_TYPE_USMALLINT:
		return SQL_SMALLINT;
	case DUCKDB_TYPE_INTEGER:
	case DUCKDB_TYPE_UINTEGER:
		return SQL_INTEGER;
	case DUCKDB_TYPE_BIGINT:
	case DUCKDB_TYPE_UBIGINT:
		return SQL_BIGINT;
	case DUCKDB_TYPE_VARCHAR:
		return SQL_VARCHAR;
	default:
		throw ScannerException("Unsupported parameter type, ID: " + std::to_string(type_id) +
		                       ", index: " + std::to_string(param_idx));
	}
}

duckdb_type Types::OdbcColumnTypeToDuck(ResultColumn &column) {
	switch (column.odbc_type.desc_concise_type) {
	case SQL_TINYINT:
		return column.odbc_type.is_unsigned ? DUCKDB_TYPE_UTINYINT : DUCKDB_TYPE_TINYINT;
	case SQL_SMALLINT:
		return column.odbc_type.is_unsigned ? DUCKDB_TYPE_USMALLINT : DUCKDB_TYPE_SMALLINT;
	case SQL_INTEGER:
		return column.odbc_type.is_unsigned ? DUCKDB_TYPE_UINTEGER : DUCKDB_TYPE_INTEGER;
	case SQL_BIGINT:
		return column.odbc_type.is_unsigned ? DUCKDB_TYPE_UBIGINT : DUCKDB_TYPE_BIGINT;
	case SQL_VARCHAR:
		return DUCKDB_TYPE_VARCHAR;
	default:
		throw ScannerException("Unsupported ODBC column type: " + column.odbc_type.ToString() + ", column name: '" +
		                       column.name + "'");
	}
}

} // namespace odbcscanner
