#include "types.hpp"

#include "diagnostics.hpp"
#include "scanner_exception.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

template <>
void TypeSpecific::BindOdbcParam<std::nullptr_t>(QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLRETURN ret = SQLBindParameter(ctx.hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_DEFAULT, param.ExpectedType(), 1, 0,
	                                 nullptr, 0, &param.LengthBytes());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException(
		    "'SQLBindParameter' NULL failed, expected type: " + std::to_string(param.ExpectedType()) +
		    " index: " + std::to_string(param_idx) + ", query: '" + ctx.query + "', return: " + std::to_string(ret) +
		    ", diagnostics: '" + diag + "'");
	}
}

void Types::SetNullValueToResult(duckdb_vector vec, idx_t row_idx) {
	duckdb_vector_ensure_validity_writable(vec);
	uint64_t *validity = duckdb_vector_get_validity(vec);
	duckdb_validity_set_row_invalid(validity, row_idx);
}

} // namespace odbcscanner
