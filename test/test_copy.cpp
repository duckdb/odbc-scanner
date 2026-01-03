#include "test_common.hpp"

#include <fstream>
#include <sstream>

static const std::string group_name = "[copy]";

TEST_CASE("Copy from file into Oracle", group_name) {
	if (!DBMSConfigured("Oracle")) {
		return;
	}

	std::ifstream fstream("resources/data/nl_stations_ora.sql", std::ios::in | std::ios::binary);
	std::stringstream sstream;
	sstream << fstream.rdbuf();
	std::string ddl = sstream.str();

	ScannerConn sc;
	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn, R"(
  SELECT * FROM odbc_query(getvariable('conn'), 'DROP TABLE "nl_train_stations"', ignore_exec_failure=TRUE)
  )",
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
	}
	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn,
		                               (R"(
  SELECT * FROM odbc_query(getvariable('conn'), ')" +
		                                ddl + R"(')
  )")
		                                   .c_str(),
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
	}
	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn, R"(
  SELECT * FROM odbc_copy_from(getvariable('conn'), 'nl_train_stations', 'resources/data/nl_stations_short.csv')
  )",
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
	}
	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn, R"(
  SELECT * FROM odbc_query(getvariable('conn'), 'SELECT count(*) FROM "nl_train_stations"')
  )",
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
		REQUIRE(res.NextChunk());
		REQUIRE(res.Value<double>(0, 0) == 3);
	}
	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn, R"(
  SELECT * FROM odbc_query(getvariable('conn'), 'SELECT "id", "code", "geo_lng" FROM "nl_train_stations" WHERE "id" = 227')
  )",
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
		REQUIRE(res.NextChunk());
		REQUIRE(res.DecimalValue<int64_t>(0, 0) == 227);
		REQUIRE(res.Value<std::string>(1, 0) == "HDE");
		REQUIRE(res.Value<int64_t>(2, 0) == 589361100);
	}
	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn, R"(
  SELECT * FROM odbc_query(getvariable('conn'), 'DROP TABLE "nl_train_stations"')
  )",
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
	}
}