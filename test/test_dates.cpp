#include "test_common.hpp"

static const std::string group_name = "[capi_dates]";

TEST_CASE("Date query with a DATE literal", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn,
	                               (R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDateSQL("''2020-12-31''") +
	                                R"(
  ')
)")
	                                   .c_str(),
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<duckdb_date_struct>(0, 0).year == 2020);
	REQUIRE(res.Value<duckdb_date_struct>(0, 0).month == 12);
	REQUIRE(res.Value<duckdb_date_struct>(0, 0).day == 31);
}

TEST_CASE("Date query with a DATE literal parameter", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn,
	                               (R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDateSQL("?", "AS col1") +
	                                R"(
  ',
	params=row('2020-12-31'::DATE))
)")
	                                   .c_str(),
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<duckdb_date_struct>(0, 0).year == 2020);
	REQUIRE(res.Value<duckdb_date_struct>(0, 0).month == 12);
	REQUIRE(res.Value<duckdb_date_struct>(0, 0).day == 31);
}

TEST_CASE("Date query with a DATE parameter", group_name) {
	ScannerConn sc;

	Result res_create_params;
	duckdb_state st_create_params = duckdb_query(sc.conn, R"(
SET VARIABLE params1 = odbc_create_params()
)",
	                                             res_create_params.Get());
	REQUIRE(QuerySuccess(res_create_params.Get(), st_create_params));

	duckdb_prepared_statement ps_ptr = nullptr;
	duckdb_state st_prepare = duckdb_prepare(sc.conn,
	                                         (R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDateSQL("?", "AS col1") +
	                                          R"(
  ', 
  params_handle=getvariable('params1'))
)")
	                                             .c_str(),
	                                         &ps_ptr);
	REQUIRE(PreparedSuccess(ps_ptr, st_prepare));
	auto ps = PreparedStatementPtr(ps_ptr, PreparedStatementDeleter);

	Result res_bind_params;
	duckdb_state st_bind_params = duckdb_query(sc.conn, R"(
SELECT odbc_bind_params(getvariable('conn'), getvariable('params1'), row('2020-12-31'::DATE))
)",
	                                           res_bind_params.Get());
	REQUIRE(QuerySuccess(res_bind_params.Get(), st_bind_params));

	Result res;
	duckdb_state st_exec = duckdb_execute_prepared(ps.get(), res.Get());

	REQUIRE(st_exec == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<duckdb_date_struct>(0, 0).year == 2020);
	REQUIRE(res.Value<duckdb_date_struct>(0, 0).month == 12);
	REQUIRE(res.Value<duckdb_date_struct>(0, 0).day == 31);
}
