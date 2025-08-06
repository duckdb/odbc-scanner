#include "test_common.hpp"

TEST_CASE("Basic query with a single value", "[basic]") {
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
	REQUIRE(res.Int32(0, 0) == 42);
}

TEST_CASE("Basic query with multiple rows and columns", "[basic]") {
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
	REQUIRE(res.String(0, 0) == "foo");
	REQUIRE(res.Int32(1, 0) == 41);
	REQUIRE(res.String(0, 1) == "foo");
	REQUIRE(res.Int32(1, 1) == 42);
}
