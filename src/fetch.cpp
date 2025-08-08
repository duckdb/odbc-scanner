#include "common.hpp"
#include "scanner_exception.hpp"
#include "types/type_integer.hpp"
#include "types/type_null.hpp"
#include "types/type_varchar.hpp"
#include "types/types.hpp"
#include "widechar.hpp"

#include <vector>

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

/*
static void FetchBigInt(const std::string &query, HSTMT hstmt, SQLSMALLINT col_idx, duckdb_vector vec, idx_t row_idx) {
    int64_t fetched = 0;
    SQLRETURN ret = SQLGetData(hstmt, col_idx, SQL_C_SBIGINT, &fetched, sizeof(fetched), nullptr);
    if (!SQL_SUCCEEDED(ret)) {
        std::string diag = ReadDiagnostics(hstmt, SQL_HANDLE_STMT);
        throw ScannerException("'SQLGetData' for BIGINT failed, column index: " + std::to_string(col_idx) +
                               ", query: '" + query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag +
                               "'");
    }

    int64_t *vec_data = reinterpret_cast<int64_t *>(duckdb_vector_get_data(vec));
    vec_data[row_idx] = fetched;
}
*/

void FetchIntoVector(const std::string &query, HSTMT hstmt, SQLSMALLINT col_idx, const OdbcType &odbc_type,
                     duckdb_vector vec, idx_t row_idx) {
	bool null_res = false;
	switch (odbc_type.desc_concise_type) {
	case SQL_INTEGER: {
		auto fetched = FetchInteger(query, hstmt, col_idx);
		null_res = fetched.second;
		if (!null_res) {
			SetIntegerResult(vec, row_idx, fetched.first);
		}
		break;
	}
	case SQL_VARCHAR: {
		auto fetched = FetchVarchar(query, hstmt, col_idx);
		null_res = fetched.second;
		if (!null_res) {
			SetVarcharResult(vec, row_idx, fetched.first);
		}
		break;
	}
	default:
		throw ScannerException("Unsupported ODBC fetch type: " + std::to_string(odbc_type.desc_concise_type) +
		                       ", name: '" + odbc_type.desc_type_name + "'");
	}

	// SQL_NULL_DATA
	if (null_res) {
		SetNullResult(vec, row_idx);
	}
}

} // namespace odbcscanner