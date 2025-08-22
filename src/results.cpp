#include "results.hpp"

#include <vector>

#include "diagnostics.hpp"
#include "scanner_exception.hpp"
#include "types.hpp"
#include "widechar.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

void Results::FetchIntoVector(const std::string &query, HSTMT hstmt, SQLSMALLINT col_idx, const OdbcType &odbc_type,
                              duckdb_vector vec, idx_t row_idx) {
	bool null_res = false;
	switch (odbc_type.desc_concise_type) {
	case SQL_INTEGER: {
		auto fetched = Types::FetchOdbcValue<int32_t>(query, hstmt, col_idx);
		null_res = fetched.second;
		if (!null_res) {
			Types::SetValueToResult<int32_t>(vec, row_idx, fetched.first);
		}
		break;
	}
	case SQL_VARCHAR: {
		auto fetched = Types::FetchOdbcValue<std::string>(query, hstmt, col_idx);
		null_res = fetched.second;
		if (!null_res) {
			Types::SetValueToResult<std::string>(vec, row_idx, fetched.first);
		}
		break;
	}
	default:
		throw ScannerException("Unsupported ODBC fetch type: " + std::to_string(odbc_type.desc_concise_type) +
		                       ", name: '" + odbc_type.desc_type_name + "'");
	}

	// SQL_NULL_DATA
	if (null_res) {
		Types::SetNullValueToResult(vec, row_idx);
	}
}

} // namespace odbcscanner
