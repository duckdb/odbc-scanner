#include "test_common.hpp"

#include <chrono>
#include <thread>

static const std::string group_name = "[capi_cancel]";

static bool EndsWith(const std::string &str, const std::string &suffix) {
	return str.size() >= suffix.size() && str.rfind(suffix) == str.size() - suffix.size();
}

TEST_CASE("Query cancellation", group_name) {
	if (!(DBMSConfigured("MSSQL"))) {
		return;
	}

	ScannerConn sc1;

	int64_t conn_handle = 0;
	{
		Result res;
		duckdb_state st = duckdb_query(sc1.conn,
		                               R"(
	SELECT getvariable('conn')
  )",
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
		REQUIRE(res.NextChunk());
		conn_handle = res.Value<int64_t>(0, 0);
	}

	Result res1;
	duckdb_state st1;
	std::thread th([&sc1, &res1, &st1] {
		st1 = duckdb_query(sc1.conn, R"(
  SELECT * FROM odbc_query(
    getvariable('conn'), 
    '
      WAITFOR DELAY ''00:00:10''
    '
    )
  )",
		                   res1.Get());
	});

	auto start = std::chrono::steady_clock::now();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	ScannerConn sc2;
	{
		Result res;
		duckdb_state st = duckdb_query(sc2.conn,
		                               std::string(R"(
	SELECT odbc_cancel_query()" + std::to_string(conn_handle) +
		                                           R"()
  )")
		                                   .c_str(),
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
		REQUIRE(res.NextChunk());
		REQUIRE(res.Value<bool>(0, 0));
	}
	th.join();
	auto end = std::chrono::steady_clock::now();
	auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	REQUIRE(st1 == DuckDBError);
	std::string res1_err = duckdb_result_error(res1.Get());
	REQUIRE(EndsWith(res1_err, "]Operation canceled'"));
	REQUIRE(elapsed_ms < 2000);
}

TEST_CASE("Query cancellation failure", group_name) {
	ScannerConn sc;
	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn,
		                               R"(
    SELECT odbc_cancel_query(NULL)
	)",
		                               res.Get());
		REQUIRE(st == DuckDBError);
	}
	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn,
		                               R"(
    SELECT odbc_cancel_query(42)
	)",
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
		REQUIRE(res.NextChunk());
		REQUIRE(!res.Value<bool>(0, 0));
	}
	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn,
		                               R"(
    SELECT odbc_cancel_query(getvariable('conn'))
	)",
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
		REQUIRE(res.NextChunk());
		REQUIRE(!res.Value<bool>(0, 0));
	}
}