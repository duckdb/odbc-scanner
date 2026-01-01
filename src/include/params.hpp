#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "binary.hpp"
#include "dbms_quirks.hpp"
#include "duckdb_extension_api.hpp"
#include "odbc_api.hpp"
#include "query_context.hpp"
#include "scanner_value.hpp"

namespace odbcscanner {

struct Params {
	static const param_type TYPE_DECIMAL_AS_CHARS = DUCKDB_TYPE_DECIMAL + 1000;
	static const param_type TYPE_TIME_WITH_NANOS = DUCKDB_TYPE_TIME + 1000;

	static std::vector<ScannerValue> Extract(DbmsQuirks &quirks, duckdb_data_chunk chunk, idx_t col_idx);

	static std::vector<ScannerValue> Extract(DbmsQuirks &quirks, duckdb_value struct_value);

	static std::vector<SQLSMALLINT> CollectTypes(QueryContext &ctx);

	static void SetExpectedTypes(QueryContext &ctx, const std::vector<SQLSMALLINT> &expected,
	                             std::vector<ScannerValue> &actual);

	static void BindToOdbc(QueryContext &ctx, std::vector<ScannerValue> &params);
};

} // namespace odbcscanner
