#include "test_common.hpp"

TEST_CASE("Basic query with a single value", "[capi_basic]") {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
	getvariable('conn'), 
	'
		SELECT 42::INT
	'
	)
)",
	                               res.Get());
	REQUIRE(st == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<int32_t>(0, 0) == 42);
}

TEST_CASE("Basic query with multiple rows and columns", "[capi_basic]") {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
	getvariable('conn'),
	'
		SELECT ''foo'', unnest([41::INT, 42::INT])
	')
)",
	                               res.Get());
	REQUIRE(st == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<std::string>(0, 0) == "foo");
	REQUIRE(res.Value<int32_t>(1, 0) == 41);
	REQUIRE(res.Value<std::string>(0, 1) == "foo");
	REQUIRE(res.Value<int32_t>(1, 1) == 42);
}
