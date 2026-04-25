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

// Signature of a bound parameter set, used to detect stable shapes across
// consecutive SQLExecute calls. When every slot matches the previous bind, the
// full SQLFreeStmt(SQL_RESET_PARAMS) + per-param SQLBindParameter cycle can be
// skipped — the driver reads the new values from the already-bound addresses
// on the next SQLExecute. Rebinding every row is the execute shape that turned
// the Firebird ODBC driver's numeric-write bug (duckdb/odbc-scanner#161 /
// FirebirdSQL/firebird-odbc-driver#292) into catastrophic row loss, and it is
// also the least efficient of the three common execute shapes.
struct BindSlotShape {
	param_type type_id = DUCKDB_TYPE_INVALID;
	SQLSMALLINT expected_type = SQL_PARAM_TYPE_UNKNOWN;

	bool operator==(const BindSlotShape &other) const {
		return type_id == other.type_id && expected_type == other.expected_type;
	}
	bool operator!=(const BindSlotShape &other) const {
		return !(*this == other);
	}
};

struct BindCache {
	std::vector<BindSlotShape> shape;
	bool initialized = false;

	void Reset() {
		shape.clear();
		initialized = false;
	}
};

struct Params {
	static const param_type TYPE_DECIMAL_AS_CHARS = DUCKDB_TYPE_DECIMAL + 1000;
	static const param_type TYPE_TIME_WITH_NANOS = DUCKDB_TYPE_TIME + 1000;
	static const param_type TYPE_SQL_BIT = DUCKDB_TYPE_BOOLEAN + 1000;
	static const param_type TYPE_SQL_GUID = DUCKDB_TYPE_UUID + 1000;
	static const param_type TYPE_SS_TIMESTAMPOFFSET = DUCKDB_TYPE_TIME_TZ + 1000;

	static std::vector<ScannerValue> Extract(DbmsQuirks &quirks, duckdb_data_chunk chunk, idx_t col_idx);

	static std::vector<ScannerValue> Extract(DbmsQuirks &quirks, duckdb_value struct_value);

	static std::vector<SQLSMALLINT> CollectTypes(QueryContext &ctx);

	static void SetExpectedTypes(QueryContext &ctx, const std::vector<SQLSMALLINT> &expected,
	                             std::vector<ScannerValue> &actual);

	static void BindToOdbc(QueryContext &ctx, std::vector<ScannerValue> &params);

	// Binds only when the current shape differs from the cached one. Returns
	// true when the cached bindings were reused (no SQLBindParameter issued),
	// false when a full rebind happened. Variable-length slots (VARCHAR /
	// TYPE_DECIMAL_AS_CHARS / BLOB / UUID) force a rebind because their backing
	// buffers are not guaranteed to keep a stable address when the value changes
	// — for those, the caller still benefits from the normal BindToOdbc path but
	// pays the bind cost every row. This includes integer / float parameters
	// whose target column is CHAR/VARCHAR/WCHAR family: SetExpectedTypes
	// coalesces those into TYPE_DECIMAL_AS_CHARS, so they take the same
	// rebind-per-row fallback.
	static bool BindToOdbcIfShapeUnchanged(QueryContext &ctx, std::vector<ScannerValue> &params, BindCache &cache);
};

} // namespace odbcscanner
