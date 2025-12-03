#include "test_common.hpp"

static const std::string group_name = "[capi_list_env]";

TEST_CASE("List drivers", group_name) {
	ScannerConn sc(false);
	Result res;
	duckdb_state st = duckdb_query(sc.conn,
	                               R"(
SELECT * FROM odbc_list_drivers()
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
}
