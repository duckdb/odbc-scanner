#include "test_common.hpp"

TEST_CASE("Date query with a DATE literal", "[capi_dates]") {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ''2020-12-31''::DATE
  ')
)",
	                               res.Get());
	REQUIRE(st == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<duckdb_date_struct>(0, 0).year == 2020);
	REQUIRE(res.Value<duckdb_date_struct>(0, 0).month == 12);
	REQUIRE(res.Value<duckdb_date_struct>(0, 0).day == 31);
}

TEST_CASE("Date query with a DATE literal parameter", "[capi_dates]") {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ?::DATE
  ',
	params=row('2020-12-31'::DATE))
)",
	                               res.Get());
	REQUIRE(st == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<duckdb_date_struct>(0, 0).year == 2020);
	REQUIRE(res.Value<duckdb_date_struct>(0, 0).month == 12);
	REQUIRE(res.Value<duckdb_date_struct>(0, 0).day == 31);
}

TEST_CASE("Date query with a DATE parameter", "[capi_dates]") {
	ScannerConn sc;

	duckdb_state st_create_params = duckdb_query(sc.conn, R"(
SET VARIABLE params1 = odbc_create_params()
)",
	                                             nullptr);
	REQUIRE(st_create_params == DuckDBSuccess);

	duckdb_prepared_statement ps_ptr = nullptr;
	duckdb_state st_prepare = duckdb_prepare(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ?::DATE
  ', 
  params_handle=getvariable('params1'))
)",
	                                         &ps_ptr);
	REQUIRE(st_prepare == DuckDBSuccess);
	auto ps = PreparedStatementPtr(ps_ptr, PreparedStatementDeleter);

	duckdb_state st_bind_params = duckdb_query(sc.conn, R"(
SELECT odbc_bind_params(getvariable('params1'), row('2020-12-31'::DATE))
)",
	                                           nullptr);
	REQUIRE(st_bind_params == DuckDBSuccess);

	Result res;
	duckdb_state st_exec = duckdb_execute_prepared(ps.get(), res.Get());

	REQUIRE(st_exec == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<duckdb_date_struct>(0, 0).year == 2020);
	REQUIRE(res.Value<duckdb_date_struct>(0, 0).month == 12);
	REQUIRE(res.Value<duckdb_date_struct>(0, 0).day == 31);
}
