#include "params.hpp"

#include "common.hpp"
#include "scanner_exception.hpp"
#include "widechar.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

ScannerParam::ScannerParam() : type_id(DUCKDB_TYPE_SQLNULL) {
}

ScannerParam::ScannerParam(int32_t value) : type_id(DUCKDB_TYPE_INTEGER), int32(value), len_bytes(sizeof(value)) {
}

ScannerParam::ScannerParam(int64_t value) : type_id(DUCKDB_TYPE_BIGINT), int64(value), len_bytes(sizeof(value)) {
}

ScannerParam::ScannerParam(std::string value)
    : type_id(DUCKDB_TYPE_VARCHAR), str(std::move(value)), wstr(utf8_to_utf16_lenient(str.data(), str.length())),
      len_bytes(wstr.length<SQLLEN>() * sizeof(SQLWCHAR)) {
}

static void SetNull(const std::string &query, HSTMT hstmt, SQLSMALLINT param_idx) {
	SQLLEN ind = SQL_NULL_DATA;
	SQLRETURN ret = SQLBindParameter(hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 0, 0, NULL, 0, &ind);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = ReadDiagnostics(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' NULL failed, index: " + std::to_string(param_idx) + ", query: '" +
		                       query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

static void SetInteger(const std::string &query, HSTMT hstmt, ScannerParam &param, SQLSMALLINT param_idx) {
	SQLRETURN ret = SQLBindParameter(hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0,
	                                 reinterpret_cast<SQLPOINTER>(&param.int32), param.len_bytes, &param.len_bytes);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = ReadDiagnostics(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' INTEGER failed, value: " + std::to_string(param.int32) +
		                       ", index: " + std::to_string(param_idx) + ", query: '" + query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

static void SetVarchar(const std::string &query, HSTMT hstmt, ScannerParam &param, SQLSMALLINT param_idx) {
	SQLRETURN ret =
	    SQLBindParameter(hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR, param.len_bytes, 0,
	                     reinterpret_cast<SQLPOINTER>(param.wstr.data()), param.len_bytes, &param.len_bytes);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = ReadDiagnostics(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' VARCHAR failed, value: '" + param.str +
		                       "', index: " + std::to_string(param_idx) + ", query: '" + query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

void SetOdbcParam(const std::string &query, HSTMT hstmt, ScannerParam &param, SQLSMALLINT param_idx) {
	switch (param.type_id) {
	case DUCKDB_TYPE_SQLNULL: {
		SetNull(query, hstmt, param_idx);
		return;
	}
	case DUCKDB_TYPE_INTEGER: {
		SetInteger(query, hstmt, param, param_idx);
		return;
	}
	case DUCKDB_TYPE_VARCHAR: {
		SetVarchar(query, hstmt, param, param_idx);
		return;
	}
	default: {
		throw ScannerException("Unsupported parameter type: " + std::to_string(param.type_id));
	}
	}
}

ScannerParam ExtractInputParam(duckdb_data_chunk input, idx_t col_idx) {
	std::string err_prefix = "Cannot extract input parameter, column: " + std::to_string(col_idx);

	auto vec = duckdb_data_chunk_get_vector(input, col_idx);
	if (vec == nullptr) {
		throw ScannerException(err_prefix + "vector is NULL");
	}

	// DUCKDB_TYPE_SQLNULL
	uint64_t *validity = duckdb_vector_get_validity(vec);
	if (validity != nullptr && !duckdb_validity_row_is_valid(validity, 0)) {
		return ScannerParam();
	}

	auto ltype = LogicalTypePtr(duckdb_vector_get_column_type(vec), LogicalTypeDeleter);
	auto type_id = duckdb_get_type_id(ltype.get());
	switch (type_id) {
	case DUCKDB_TYPE_INTEGER: {
		int32_t *data = reinterpret_cast<int32_t *>(duckdb_vector_get_data(vec));
		int32_t num = data[0];
		return ScannerParam(num);
	}
	case DUCKDB_TYPE_VARCHAR: {
		duckdb_string_t *data = reinterpret_cast<duckdb_string_t *>(duckdb_vector_get_data(vec));
		duckdb_string_t dstr = data[0];
		const char *cstr = duckdb_string_t_data(&dstr);
		uint32_t len = duckdb_string_t_length(dstr);
		return ScannerParam(std::string(cstr, len));
	}
	default: {
		throw ScannerException("Unsupported parameter type: " + std::to_string(type_id));
	}
	}
}

} // namespace odbcscanner