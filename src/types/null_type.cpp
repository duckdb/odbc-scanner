#include "types.hpp"

#include "diagnostics.hpp"
#include "scanner_exception.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

template <>
void TypeSpecific::BindOdbcParam<std::nullptr_t>(const std::string &query, HSTMT hstmt, ScannerParam &param,
                                                 SQLSMALLINT param_idx) {
	(void)param;
	SQLLEN ind = SQL_NULL_DATA;
	SQLRETURN ret = SQLBindParameter(hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 0, 0, NULL, 0, &ind);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' NULL failed, index: " + std::to_string(param_idx) + ", query: '" +
		                       query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

void Types::SetNullValueToResult(duckdb_vector vec, idx_t row_idx) {
	duckdb_vector_ensure_validity_writable(vec);
	uint64_t *validity = duckdb_vector_get_validity(vec);
	duckdb_validity_set_row_invalid(validity, row_idx);
}

} // namespace odbcscanner
