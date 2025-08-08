#include "types/type_null.hpp"

#include "common.hpp"
#include "scanner_exception.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

void SetNullParam(const std::string &query, HSTMT hstmt, SQLSMALLINT param_idx) {
	SQLLEN ind = SQL_NULL_DATA;
	SQLRETURN ret = SQLBindParameter(hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 0, 0, NULL, 0, &ind);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = ReadDiagnostics(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' NULL failed, index: " + std::to_string(param_idx) + ", query: '" +
		                       query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

void SetNullResult(duckdb_vector vec, idx_t row_idx) {
	duckdb_vector_ensure_validity_writable(vec);
	uint64_t *validity = duckdb_vector_get_validity(vec);
	duckdb_validity_set_row_invalid(validity, row_idx);
}

} // namespace odbcscanner
