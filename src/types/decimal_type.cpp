#include "types.hpp"

#include <cstdint>
#include <cstring>

#include "capi_pointers.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<duckdb_decimal>(duckdb_vector vec) {
	auto ltype = LogicalTypePtr(duckdb_vector_get_column_type(vec), LogicalTypeDeleter);

	duckdb_decimal dec;
	std::memset(&dec, '\0', sizeof(dec));
	dec.width = duckdb_decimal_width(ltype.get());
	dec.scale = duckdb_decimal_scale(ltype.get());

	duckdb_type type_id = duckdb_decimal_internal_type(ltype.get());
	switch (type_id) {
	case DUCKDB_TYPE_SMALLINT: {
		int16_t *data = reinterpret_cast<int16_t *>(duckdb_vector_get_data(vec));
		dec.value.lower = static_cast<uint64_t>(data[0]);
		break;
	}
	case DUCKDB_TYPE_INTEGER: {
		int32_t *data = reinterpret_cast<int32_t *>(duckdb_vector_get_data(vec));
		dec.value.lower = static_cast<uint64_t>(data[0]);
		break;
	}
	case DUCKDB_TYPE_BIGINT: {
		int64_t *data = reinterpret_cast<int64_t *>(duckdb_vector_get_data(vec));
		dec.value.lower = static_cast<uint64_t>(data[0]);
		break;
	}
	case DUCKDB_TYPE_HUGEINT: {
		duckdb_hugeint *data = reinterpret_cast<duckdb_hugeint *>(duckdb_vector_get_data(vec));
		dec.value = data[0];
		break;
	}
	default:
		throw ScannerException("Invalid unsupported DECIMAL internal type, ID: " + std::to_string(type_id));
	}

	return ScannerParam(dec);
}

template <>
void TypeSpecific::BindOdbcParam<duckdb_decimal>(const std::string &query, HSTMT hstmt, ScannerParam &param,
                                                 SQLSMALLINT param_idx) {
	SQL_NUMERIC_STRUCT &ns = param.Value<SQL_NUMERIC_STRUCT>();
	SQLRETURN ret = SQLBindParameter(hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_NUMERIC, SQL_NUMERIC,
	                                 static_cast<SQLULEN>(ns.precision), static_cast<SQLSMALLINT>(ns.scale),
	                                 reinterpret_cast<SQLPOINTER>(&ns), param.LengthBytes(), &param.LengthBytes());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' failed, type: " + std::to_string(SQL_DECIMAL) +
		                       ", index: " + std::to_string(param_idx) + ", query: '" + query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

template <>
void TypeSpecific::FetchAndSetResult<duckdb_decimal>(OdbcType &odbc_type, const std::string &query, HSTMT hstmt,
                                                     SQLSMALLINT col_idx, duckdb_vector vec, idx_t row_idx) {
	SQL_NUMERIC_STRUCT fetched;
	std::memset(&fetched, '\0', sizeof(fetched));
	SQLLEN ind;
	SQLRETURN ret = SQLGetData(hstmt, col_idx, SQL_C_NUMERIC, &fetched, sizeof(fetched), &ind);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' for failed, C type: " + std::to_string(SQL_C_NUMERIC) +
		                       ", column index: " + std::to_string(col_idx) + ", column type: " + odbc_type.ToString() +
		                       ",  query: '" + query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag +
		                       "'");
	}

	if (ind == SQL_NULL_DATA) {
		Types::SetNullValueToResult(vec, row_idx);
		return;
	}

	duckdb_hugeint hi;
	std::memcpy(&hi.lower, fetched.val, sizeof(hi.lower));
	std::memcpy(&hi.upper, fetched.val + sizeof(hi.lower), sizeof(hi.upper));

	// only used for <= 64 bit
	int64_t signed_lower = static_cast<int64_t>(hi.lower);
	if (fetched.sign == 0) {
		signed_lower = -static_cast<int64_t>(hi.lower);
		// negate
		hi.lower = ~hi.lower + 1;
		hi.upper = ~hi.upper;
		if (hi.lower == 0) {
			hi.upper += 1; // carry from low to high
		}
	}

	if (odbc_type.decimal_precision <= 4) {
		int16_t *data = reinterpret_cast<int16_t *>(duckdb_vector_get_data(vec));
		data[row_idx] = static_cast<int16_t>(signed_lower);
	} else if (odbc_type.decimal_precision <= 9) {
		int32_t *data = reinterpret_cast<int32_t *>(duckdb_vector_get_data(vec));
		data[row_idx] = static_cast<int32_t>(signed_lower);
	} else if (odbc_type.decimal_precision <= 18) {
		int64_t *data = reinterpret_cast<int64_t *>(duckdb_vector_get_data(vec));
		data[row_idx] = static_cast<int64_t>(signed_lower);
	} else if (odbc_type.decimal_precision <= 38) {
		duckdb_hugeint *data = reinterpret_cast<duckdb_hugeint *>(duckdb_vector_get_data(vec));
		data[row_idx] = hi;
	} else
		throw ScannerException("Invalid unsupported DECIMAL precision: " + std::to_string(odbc_type.decimal_precision) +
		                       ", column index: " + std::to_string(col_idx) + ", column type: " + odbc_type.ToString() +
		                       ",  query: '" + query);
}

} // namespace odbcscanner
