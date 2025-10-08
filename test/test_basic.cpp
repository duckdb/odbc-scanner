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

TEST_CASE("Basic query with a prepared statement", group_name) {
	ScannerConn sc;
	duckdb_prepared_statement ps_ptr = nullptr;
	duckdb_state st_prepare = duckdb_prepare(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
		SELECT CAST(42 AS INTEGER)
  ')
)",
	                                         &ps_ptr);
	REQUIRE(PreparedSuccess(ps_ptr, st_prepare));
	auto ps = PreparedStatementPtr(ps_ptr, PreparedStatementDeleter);

	{
		Result res;
		duckdb_state st_exec = duckdb_execute_prepared(ps.get(), res.Get());
		REQUIRE(QuerySuccess(res.Get(), st_exec));
		REQUIRE(res.NextChunk());
		REQUIRE(res.Value<int32_t>(0, 0) == 42);
	}

	{
		Result res;
		duckdb_state st_exec = duckdb_execute_prepared(ps.get(), res.Get());
		REQUIRE(QuerySuccess(res.Get(), st_exec));
		REQUIRE(res.NextChunk());
		REQUIRE(res.Value<int32_t>(0, 0) == 42);
	}
}
