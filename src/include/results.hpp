#pragma once

#include <sql.h>
#include <sqlext.h>
#include <string>

#include "duckdb_extension.h"
#include "types.hpp"

namespace odbcscanner {

struct Results {
	static void FetchIntoVector(const std::string &query, HSTMT hstmt, SQLSMALLINT col_idx, const OdbcType &odbc_type,
	                            duckdb_vector vec, idx_t row_idx);
};

} // namespace odbcscanner
