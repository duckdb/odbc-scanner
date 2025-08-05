#include "common.hpp"
#include "scanner_exception.hpp"
#include "types.hpp"
#include "widechar.hpp"

#include <vector>

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

static void FetchInteger(const std::string &query, HSTMT hstmt, SQLSMALLINT col_idx, duckdb_vector vec, idx_t row_idx) {
	int32_t fetched = 0;
	SQLRETURN ret = SQLGetData(hstmt, col_idx, SQL_C_SLONG, &fetched, sizeof(fetched), nullptr);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = ReadDiagnostics(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' for INTEGER failed, column index: " + std::to_string(col_idx) +
		                       ", query: '" + query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag +
		                       "'");
	}

	int64_t *vec_data = reinterpret_cast<int64_t *>(duckdb_vector_get_data(vec));
	vec_data[row_idx] = fetched;
}

static void FetchVarchar(const std::string &query, HSTMT hstmt, SQLSMALLINT col_idx, duckdb_vector vec, idx_t row_idx) {
	// todo: parts
	std::vector<SQLWCHAR> buf;
	buf.resize(1024);
	SQLLEN len = 0;
	SQLRETURN ret = SQLGetData(hstmt, col_idx, SQL_C_WCHAR, buf.data(), static_cast<SQLSMALLINT>(buf.size() * 2), &len);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = ReadDiagnostics(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' for VARCHAR failed, column index: " + std::to_string(col_idx) +
		                       ", query: '" + query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag +
		                       "'");
	}

	std::string str = utf16_to_utf8_lenient(buf.data(), len / 2);
	duckdb_vector_assign_string_element_len(vec, row_idx, str.c_str(), str.length());
}

void FetchIntoVector(SQLSMALLINT odbc_ctype, const std::string &query, HSTMT hstmt, SQLSMALLINT col_idx,
                     duckdb_vector vec, idx_t row_idx) {
	switch (odbc_ctype) {
	case SQL_INTEGER: {
		FetchInteger(query, hstmt, col_idx, vec, row_idx);
		break;
	}
	case SQL_VARCHAR: {
		FetchVarchar(query, hstmt, col_idx, vec, row_idx);
		break;
	}
	default:
		throw ScannerException("Unsupported ODBC C type: " + std::to_string(odbc_ctype));
	}
}

} // namespace odbcscanner