#include "test_common.hpp"

static const std::string group_name = "[capi_basic]";

TEST_CASE("Basic query with a single value", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
	getvariable('conn'), 
	'
		SELECT CAST(42 AS INTEGER)
	'
	)
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<int32_t>(0, 0) == 42);
}

TEST_CASE("Basic query with multiple rows and columns", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
	getvariable('conn'),
	'
		SELECT ''foo'', CAST(41 AS INTEGER)
		UNION ALL
		SELECT ''bar'', CAST(42 AS INTEGER)
	')
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<std::string>(0, 0) == "foo");
	REQUIRE(res.Value<int32_t>(1, 0) == 41);
	REQUIRE(res.Value<std::string>(0, 1) == "bar");
	REQUIRE(res.Value<int32_t>(1, 1) == 42);
}
