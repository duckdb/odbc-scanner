#include "test_common.hpp"

static const std::string group_name = "[capi_list_env]";

TEST_CASE("List drivers", group_name) {
	ScannerConn sc(false);
	Result res;
	bool st = Query(sc.conn,
	                R"(
FROM odbc_list_drivers()
)",
	                res);
	REQUIRE(QuerySuccess(res, st));
}

TEST_CASE("List data sources", group_name) {
	ScannerConn sc(false);
	Result res;
	bool st = Query(sc.conn,
	                R"(
FROM odbc_list_data_sources()
)",
	                res);
	REQUIRE(QuerySuccess(res, st));
}
