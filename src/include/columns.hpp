#pragma once

#include <string>

#include <sql.h>
#include <sqlext.h>

#include "duckdb_extension.h"

#include "types.hpp"

namespace odbcscanner {

struct ResultColumn {
	std::string name;
	OdbcType odbc_type;

	ResultColumn(std::string name_in, OdbcType odbc_type_in)
	    : name(std::move(name_in)), odbc_type(std::move(odbc_type_in)) {
	}

	ResultColumn(const ResultColumn &other) = delete;
	ResultColumn(ResultColumn &&other) = default;

	ResultColumn &operator=(const ResultColumn &other) = delete;
	ResultColumn &operator=(ResultColumn &&other) = default;
};

struct Columns {
	static std::vector<ResultColumn> Collect(const std::string &query, HSTMT hstmt);

	static void CheckSame(const std::string &query, std::vector<ResultColumn> &expected,
	                      std::vector<ResultColumn> &actual);
};

} // namespace odbcscanner
