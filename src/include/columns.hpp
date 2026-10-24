#pragma once

#include <string>

#include "duckdb_extension_api.hpp"
#include "odbc_api.hpp"
#include "query_context.hpp"
#include "scanner_value.hpp"
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
	static std::vector<ResultColumn> Collect(const QueryContext &ctx);

	static void CheckSame(const QueryContext &ctx, const std::vector<ResultColumn> &expected,
	                      const std::vector<ResultColumn> &actual);

	static void AddToResults(TableFunction::BindInput &input, Factory &factory, LogicalTypeId type_id,
	                         ResultColumn &col);
};

} // namespace odbcscanner
