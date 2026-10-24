#include "test_common.hpp"

static const std::string group_name = "[capi_dates]";

TEST_CASE("Date query with a DATE literal", group_name) {
	ScannerConn sc;
	Result res;
	bool st = Query(sc.conn,
	                (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDateSQL("''2020-12-31''") +
	                 R"(
  ')
)")
	                    .c_str(),
	                res);
	REQUIRE(QuerySuccess(res, st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<DateStruct>(0, 0).year == 2020);
	REQUIRE(res.Value<DateStruct>(0, 0).month == 12);
	REQUIRE(res.Value<DateStruct>(0, 0).day == 31);
}

TEST_CASE("Date query with a DATE literal parameter", group_name) {
	if (DBMSConfigured("FlightSQL")) {
		return;
	}
	ScannerConn sc;
	Result res;
	bool st = Query(sc.conn,
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
	                res);
	REQUIRE(QuerySuccess(res, st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<DateStruct>(0, 0).year == 2020);
	REQUIRE(res.Value<DateStruct>(0, 0).month == 12);
	REQUIRE(res.Value<DateStruct>(0, 0).day == 31);
}

TEST_CASE("Date query with a DATE parameter", group_name) {
	if (DBMSConfigured("FlightSQL")) {
		return;
	}
	ScannerConn sc;

	PreparedStatement ps;
	bool st_prepare = Prepare(sc.conn,
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
	                          ps);
	REQUIRE(PreparedSuccess(ps, st_prepare));

	DateStruct date_struct = {2020, 12, 31};
	auto param_val = CreateDate(sc, date_struct);
	bool st_bind = ps.Bind(1, param_val.get());
	REQUIRE(PreparedSuccess(ps, st_bind));

	Result res;
	bool st_exec = ps.Execute(res);

	REQUIRE(QuerySuccess(res, st_exec));
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<DateStruct>(0, 0).year == 2020);
	REQUIRE(res.Value<DateStruct>(0, 0).month == 12);
	REQUIRE(res.Value<DateStruct>(0, 0).day == 31);
}
