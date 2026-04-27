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

// Per-slot signature captured the last time a parameter index was bound. The
// pair (type_id, expected_type) is everything BindOdbcParam looks at; if both
// match the current value, the slot's previous SQLBindParameter is still valid
// (modulo buffer-pointer stability — see IsFixedWidthShape in params.cpp).
// Rebinding every row is the execute shape that turned the Firebird ODBC
// driver's numeric-write bug (duckdb/odbc-scanner#161 /
// FirebirdSQL/firebird-odbc-driver#292) into catastrophic row loss, and it is
// also the least efficient of the three common execute shapes.
struct BindSlotShape {
	param_type type_id = DUCKDB_TYPE_INVALID;
	SQLSMALLINT expected_type = SQL_PARAM_TYPE_UNKNOWN;

	BindSlotShape() = default;
	BindSlotShape(param_type type_id_in, SQLSMALLINT expected_type_in)
	    : type_id(type_id_in), expected_type(expected_type_in) {
	}

	bool operator==(const BindSlotShape &other) const {
		return type_id == other.type_id && expected_type == other.expected_type;
	}
	bool operator!=(const BindSlotShape &other) const {
		return !(*this == other);
	}
};

// Per-prepared-statement bind state. `shape[i]` is the last-bound shape for
// param index i+1; an empty vector means "nothing bound yet". Caller MUST call
// Reset() whenever the prepared statement changes — SQLPrepare does NOT clear
// parameter bindings on the hstmt, so BindToOdbcWithCache will issue
// SQL_RESET_PARAMS itself the next time it sees an empty cache.
struct BindCache {
	std::vector<BindSlotShape> shape;

	void Reset() {
		shape.clear();
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

	// Per-slot incremental bind. Issues SQLBindParameter only for slots whose
	// shape changed since the cached bind, or whose backing buffer is variable-
	// length (DecimalChars / WideString / ScannerBlob / ScannerUuid — the
	// `std::vector` inside can move on value reassignment, so the previously
	// bound pointer would dangle). Fixed-width slots whose shape is unchanged
	// keep their existing SQLBindParameter — the driver reads the new value
	// from the same address on the next SQLExecute.
	//
	// This includes integer / float parameters whose target column is the
	// CHAR/VARCHAR/WCHAR family: SetExpectedTypes coalesces those into
	// TYPE_DECIMAL_AS_CHARS, so each such row still rebinds that slot but
	// fixed-width siblings on the same row stay cached.
	//
	// In the steady state (cache populated, same `params.size()`), per-slot
	// SQLBindParameter overwrites the previous binding for that index — no
	// SQL_RESET_PARAMS needed. On the first call after Reset() (or when
	// `params.size()` changes), SQL_RESET_PARAMS is issued first to drop any
	// leftover bindings from a previous prepared statement on the same hstmt.
	// Caller must Reset(cache) whenever the prepared statement changes or the
	// params vector's storage may have moved (different `std::vector`
	// instance, capacity-changing resize, etc.).
	static void BindToOdbcWithCache(QueryContext &ctx, std::vector<ScannerValue> &params, BindCache &cache);
};

} // namespace odbcscanner
