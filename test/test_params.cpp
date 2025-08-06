#include "test_common.hpp"

TEST_CASE("Params query with a single param", "[params]") {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ?::INT
  ', 
  params=odbc_params(42))
)",
	                               res.Get());
	REQUIRE(st == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.Int32(0, 0) == 42);
}

TEST_CASE("Params query with a varchar param", "[params]") {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ?::VARCHAR
  ', 
  params=odbc_params('foo'))
)",
	                               res.Get());
	REQUIRE(st == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.String(0, 0) == "foo");
}

TEST_CASE("Params query with multiple params including NULL", "[params]") {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT coalesce(?::INT, ?::INT)
  ', 
  params=odbc_params(NULL, 42))
)",
	                               res.Get());
	REQUIRE(st == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.Int32(0, 0) == 42);
}
