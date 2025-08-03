#include "params.hpp"

#include "common.hpp"
#include "scanner_exception.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

static void SetInteger(const std::string &query, HSTMT hstmt, duckdb_value value, SQLSMALLINT param_idx,
                       std::vector<void *> &holder) {
	int32_t num = duckdb_get_int32(value);
	void *num_ptr = std::malloc(sizeof(num));
	holder.push_back(num_ptr);
	std::memcpy(num_ptr, &num, sizeof(num));

	SQLLEN num_len = sizeof(num);
	SQLRETURN ret = SQLBindParameter(hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0,
	                                 reinterpret_cast<SQLPOINTER>(num_ptr), num_len, &num_len);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = ReadDiagnostics(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' INTEGER failed, value: " + std::to_string(num) + ", query: '" +
		                       query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

void SetOdbcParam(const std::string &query, HSTMT hstmt, duckdb_value value, SQLSMALLINT param_idx,
                  std::vector<void *> &holder) {
	duckdb_logical_type val_type = duckdb_get_value_type(value);
	duckdb_type type_id = duckdb_get_type_id(val_type);

	switch (type_id) {
	case DUCKDB_TYPE_INTEGER: {
		SetInteger(query, hstmt, value, param_idx, holder);
		return;
	}
	default: {
		throw ScannerException("Unsupported parameter type: " + std::to_string(type_id));
	}
	}
}

ValuePtr ExtractInputParam(duckdb_data_chunk input, idx_t col_idx) {
	std::string err_prefix = "Cannot extract input parameter, column: " + std::to_string(col_idx);

	auto vec = duckdb_data_chunk_get_vector(input, col_idx);
	if (vec == nullptr) {
		throw ScannerException(err_prefix + "vector is NULL");
	}

	auto logical_type = duckdb_vector_get_column_type(vec);
	auto type_id = duckdb_get_type_id(logical_type);
	switch (type_id) {
	case DUCKDB_TYPE_INTEGER: {
		int32_t *data = reinterpret_cast<int32_t *>(duckdb_vector_get_data(vec));
		int32_t num = data[0];
		return ValuePtr(duckdb_create_int32(num), ValueDeleter);
	}
	default: {
		throw ScannerException("Unsupported parameter type: " + std::to_string(type_id));
	}
	}
}

} // namespace odbcscanner