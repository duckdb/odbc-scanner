#pragma once

#include <string>

#include <sql.h>
#include <sqlext.h>

#include "dbms_quirks.hpp"

namespace odbcscanner {

struct QueryContext {
	std::string query;
	HSTMT hstmt;
	DbmsQuirks quirks;

	explicit QueryContext(std::string query_in, HSTMT hstmt_in, DbmsQuirks quirks_in)
	    : query(std::move(query_in)), hstmt(hstmt_in), quirks(std::move(quirks_in)) {
	}

	QueryContext(QueryContext &other) = delete;
	QueryContext(QueryContext &&other) = default;

	QueryContext &operator=(const QueryContext &other) = delete;
	QueryContext &operator=(QueryContext &&other) = default;
};

} // namespace odbcscanner
