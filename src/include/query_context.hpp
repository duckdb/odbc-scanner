#pragma once

#include <string>
#include <vector>

#include "column_bind.hpp"
#include "dbms_quirks.hpp"
#include "odbc_api.hpp"

namespace odbcscanner {

struct QueryContext {
	std::string query;
	HSTMT hstmt;
	DbmsQuirks quirks;
	std::vector<ColumnBind> col_binds;

	explicit QueryContext(std::string query_in, HSTMT hstmt_in, DbmsQuirks quirks_in)
	    : query(std::move(query_in)), hstmt(hstmt_in), quirks(std::move(quirks_in)) {
	}

	QueryContext(QueryContext &other) = delete;
	QueryContext(QueryContext &&other) = default;

	QueryContext &operator=(const QueryContext &other) = delete;
	QueryContext &operator=(QueryContext &&other) = default;

	ColumnBind &BindForColumn(SQLSMALLINT col_idx) {
		size_t col_idxz = static_cast<size_t>(col_idx - 1);
		return col_binds.at(col_idxz);
	}
};

} // namespace odbcscanner
