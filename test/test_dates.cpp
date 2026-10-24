#include "test_common.hpp"

static const std::string group_name = "[capi_dates]";

TEST_CASE("Date query with a DATE literal", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn,
	                               (R"(
FROM odbc_query(
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
	if (DBMSConfigured("FlightSQL")) {
		return;
	}
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn,
	                               (R"(
FROM odbc_query(
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
	if (DBMSConfigured("FlightSQL")) {
		return;
	}
	ScannerConn sc;

	duckdb_prepared_statement ps_ptr = nullptr;
	duckdb_state st_prepare = duckdb_prepare(sc.conn,
	                                         (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDateSQL("?", "AS col1") +
	                                          R"(
  ', 
  params=row(?))
)")
	                                             .c_str(),
	                                         &ps_ptr);
	REQUIRE(PreparedSuccess(ps_ptr, st_prepare));
	auto ps = PreparedStatementPtr(ps_ptr, PreparedStatementDeleter);

	duckdb_date_struct date_struct = {2020, 12, 31};
	auto param_val = ValuePtr(duckdb_create_date(duckdb_to_date(date_struct)), ValueDeleter);
	duckdb_state st_bind = duckdb_bind_value(ps.get(), 1, param_val.get());
	REQUIRE(PreparedSuccess(ps_ptr, st_bind));

	Result res;
	duckdb_state st_exec = duckdb_execute_prepared(ps.get(), res.Get());

	REQUIRE(QuerySuccess(res.Get(), st_exec));
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<duckdb_date_struct>(0, 0).year == 2020);
	REQUIRE(res.Value<duckdb_date_struct>(0, 0).month == 12);
	REQUIRE(res.Value<duckdb_date_struct>(0, 0).day == 31);
}
