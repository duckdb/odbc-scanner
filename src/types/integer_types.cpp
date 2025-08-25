#include "types.hpp"

#include <cstdint>

#include "capi_pointers.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

template <typename INT_TYPE>
static std::pair<INT_TYPE, bool> ExtractFunctionArgInternal(const std::string &type_name, duckdb_data_chunk chunk,
                                                            idx_t col_idx) {
	idx_t col_count = duckdb_data_chunk_get_column_count(chunk);
	if (col_idx >= col_count) {
		throw ScannerException("Cannot extract " + type_name + " function argument: column not found, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	duckdb_vector vec = duckdb_data_chunk_get_vector(chunk, col_idx);
	if (vec == nullptr) {
		throw ScannerException("Cannot extract " + type_name + " function argument: vector is NULL, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	idx_t rows_count = duckdb_data_chunk_get_size(chunk);
	if (rows_count == 0) {
		throw ScannerException("Cannot extract " + type_name + " function argument: vector contains no rows, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	uint64_t *validity = duckdb_vector_get_validity(vec);
	if (validity != nullptr && !duckdb_validity_row_is_valid(validity, 0)) {
		return std::make_pair(0, true);
	}

	INT_TYPE *data = reinterpret_cast<INT_TYPE *>(duckdb_vector_get_data(vec));
	INT_TYPE res = data[0];
	return std::make_pair(res, false);
}

template <>
std::pair<int32_t, bool> Types::ExtractFunctionArg<int32_t>(duckdb_data_chunk chunk, idx_t col_idx) {
	return ExtractFunctionArgInternal<int32_t>("INTEGER", chunk, col_idx);
}

template <>
std::pair<int64_t, bool> Types::ExtractFunctionArg<int64_t>(duckdb_data_chunk chunk, idx_t col_idx) {
	return ExtractFunctionArgInternal<int64_t>("BIGINT", chunk, col_idx);
}

template <typename INT_TYPE>
static ScannerParam ExtractNotNullParamInternal(duckdb_vector vec) {
	INT_TYPE *data = reinterpret_cast<INT_TYPE *>(duckdb_vector_get_data(vec));
	INT_TYPE num = data[0];
	return ScannerParam(num);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<int8_t>(duckdb_vector vec) {
	return ExtractNotNullParamInternal<int8_t>(vec);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<uint8_t>(duckdb_vector vec) {
	return ExtractNotNullParamInternal<uint8_t>(vec);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<int16_t>(duckdb_vector vec) {
	return ExtractNotNullParamInternal<int16_t>(vec);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<uint16_t>(duckdb_vector vec) {
	return ExtractNotNullParamInternal<uint16_t>(vec);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<int32_t>(duckdb_vector vec) {
	return ExtractNotNullParamInternal<int32_t>(vec);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<uint32_t>(duckdb_vector vec) {
	return ExtractNotNullParamInternal<uint32_t>(vec);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<int64_t>(duckdb_vector vec) {
	return ExtractNotNullParamInternal<int64_t>(vec);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<uint64_t>(duckdb_vector vec) {
	return ExtractNotNullParamInternal<uint64_t>(vec);
}

template <typename INT_TYPE>
static void BindOdbcParamInternal(SQLSMALLINT ctype, SQLSMALLINT sqltype, const std::string &query, HSTMT hstmt,
                                  ScannerParam &param, SQLSMALLINT param_idx) {
	SQLRETURN ret = SQLBindParameter(hstmt, param_idx, SQL_PARAM_INPUT, ctype, sqltype, 0, 0,
	                                 reinterpret_cast<SQLPOINTER>(&param.Value<INT_TYPE>()), param.LengthBytes(),
	                                 &param.LengthBytes());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' failed, type: " + std::to_string(sqltype) +
		                       ", value: " + std::to_string(param.Value<INT_TYPE>()) +
		                       ", index: " + std::to_string(param_idx) + ", query: '" + query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

template <>
void TypeSpecific::BindOdbcParam<int8_t>(const std::string &query, HSTMT hstmt, ScannerParam &param,
                                         SQLSMALLINT param_idx) {
	BindOdbcParamInternal<int8_t>(SQL_C_STINYINT, SQL_TINYINT, query, hstmt, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<uint8_t>(const std::string &query, HSTMT hstmt, ScannerParam &param,
                                          SQLSMALLINT param_idx) {
	BindOdbcParamInternal<uint8_t>(SQL_C_UTINYINT, SQL_TINYINT, query, hstmt, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<int16_t>(const std::string &query, HSTMT hstmt, ScannerParam &param,
                                          SQLSMALLINT param_idx) {
	BindOdbcParamInternal<int16_t>(SQL_C_SSHORT, SQL_SMALLINT, query, hstmt, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<uint16_t>(const std::string &query, HSTMT hstmt, ScannerParam &param,
                                           SQLSMALLINT param_idx) {
	BindOdbcParamInternal<uint16_t>(SQL_C_USHORT, SQL_SMALLINT, query, hstmt, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<int32_t>(const std::string &query, HSTMT hstmt, ScannerParam &param,
                                          SQLSMALLINT param_idx) {
	BindOdbcParamInternal<int32_t>(SQL_C_SLONG, SQL_INTEGER, query, hstmt, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<uint32_t>(const std::string &query, HSTMT hstmt, ScannerParam &param,
                                           SQLSMALLINT param_idx) {
	BindOdbcParamInternal<uint32_t>(SQL_C_ULONG, SQL_INTEGER, query, hstmt, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<int64_t>(const std::string &query, HSTMT hstmt, ScannerParam &param,
                                          SQLSMALLINT param_idx) {
	BindOdbcParamInternal<int64_t>(SQL_C_SBIGINT, SQL_BIGINT, query, hstmt, param, param_idx);
}

template <>
void TypeSpecific::BindOdbcParam<uint64_t>(const std::string &query, HSTMT hstmt, ScannerParam &param,
                                           SQLSMALLINT param_idx) {
	BindOdbcParamInternal<uint64_t>(SQL_C_UBIGINT, SQL_BIGINT, query, hstmt, param, param_idx);
}

template <typename INT_TYPE>
static void FetchAndSetResultInternal(SQLSMALLINT ctype, OdbcType &odbc_type, const std::string &query, HSTMT hstmt,
                                      SQLSMALLINT col_idx, duckdb_vector vec, idx_t row_idx) {
	INT_TYPE fetched = 0;
	SQLLEN ind;
	SQLRETURN ret = SQLGetData(hstmt, col_idx, ctype, &fetched, sizeof(fetched), &ind);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' for failed, C type: " + std::to_string(ctype) + ", column index: " +
		                       std::to_string(col_idx) + ", column type: " + odbc_type.ToString() + ",  query: '" +
		                       query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}

	if (ind == SQL_NULL_DATA) {
		Types::SetNullValueToResult(vec, row_idx);
		return;
	}

	INT_TYPE *data = reinterpret_cast<INT_TYPE *>(duckdb_vector_get_data(vec));
	data[row_idx] = fetched;
}

template <>
void TypeSpecific::FetchAndSetResult<int8_t>(OdbcType &odbc_type, const std::string &query, HSTMT hstmt,
                                             SQLSMALLINT col_idx, duckdb_vector vec, idx_t row_idx) {
	FetchAndSetResultInternal<int8_t>(SQL_C_STINYINT, odbc_type, query, hstmt, col_idx, vec, row_idx);
}

template <>
void TypeSpecific::FetchAndSetResult<uint8_t>(OdbcType &odbc_type, const std::string &query, HSTMT hstmt,
                                              SQLSMALLINT col_idx, duckdb_vector vec, idx_t row_idx) {
	FetchAndSetResultInternal<uint8_t>(SQL_C_UTINYINT, odbc_type, query, hstmt, col_idx, vec, row_idx);
}

template <>
void TypeSpecific::FetchAndSetResult<int16_t>(OdbcType &odbc_type, const std::string &query, HSTMT hstmt,
                                              SQLSMALLINT col_idx, duckdb_vector vec, idx_t row_idx) {
	FetchAndSetResultInternal<int16_t>(SQL_C_SSHORT, odbc_type, query, hstmt, col_idx, vec, row_idx);
}

template <>
void TypeSpecific::FetchAndSetResult<uint16_t>(OdbcType &odbc_type, const std::string &query, HSTMT hstmt,
                                               SQLSMALLINT col_idx, duckdb_vector vec, idx_t row_idx) {
	FetchAndSetResultInternal<uint16_t>(SQL_C_USHORT, odbc_type, query, hstmt, col_idx, vec, row_idx);
}

template <>
void TypeSpecific::FetchAndSetResult<int32_t>(OdbcType &odbc_type, const std::string &query, HSTMT hstmt,
                                              SQLSMALLINT col_idx, duckdb_vector vec, idx_t row_idx) {
	FetchAndSetResultInternal<int32_t>(SQL_C_SLONG, odbc_type, query, hstmt, col_idx, vec, row_idx);
}

template <>
void TypeSpecific::FetchAndSetResult<uint32_t>(OdbcType &odbc_type, const std::string &query, HSTMT hstmt,
                                               SQLSMALLINT col_idx, duckdb_vector vec, idx_t row_idx) {
	FetchAndSetResultInternal<uint32_t>(SQL_C_ULONG, odbc_type, query, hstmt, col_idx, vec, row_idx);
}

template <>
void TypeSpecific::FetchAndSetResult<int64_t>(OdbcType &odbc_type, const std::string &query, HSTMT hstmt,
                                              SQLSMALLINT col_idx, duckdb_vector vec, idx_t row_idx) {
	FetchAndSetResultInternal<int64_t>(SQL_C_SBIGINT, odbc_type, query, hstmt, col_idx, vec, row_idx);
}

template <>
void TypeSpecific::FetchAndSetResult<uint64_t>(OdbcType &odbc_type, const std::string &query, HSTMT hstmt,
                                               SQLSMALLINT col_idx, duckdb_vector vec, idx_t row_idx) {
	FetchAndSetResultInternal<uint64_t>(SQL_C_UBIGINT, odbc_type, query, hstmt, col_idx, vec, row_idx);
}

} // namespace odbcscanner
