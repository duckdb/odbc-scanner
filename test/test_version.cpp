#include "test_common.hpp"

static const std::string group_name = "[capi_version]";

TEST_CASE("DuckDB version query", group_name) {
	if (!DBMSConfigured("DuckDB")) {
		return;
	}
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
	getvariable('conn'), 
	'
		SELECT version()
	'
	)
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	std::cout << res.Value<std::string>(0, 0) << std::endl;
}

TEST_CASE("MSSQL version query", group_name) {
	if (!DBMSConfigured("MSSQL")) {
		return;
	}
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
	getvariable('conn'), 
	'
		SELECT @@version
	'
	)
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	std::cout << res.Value<std::string>(0, 0) << std::endl;
}

TEST_CASE("Postgres version query", group_name) {
	if (!DBMSConfigured("PostgreSQL")) {
		return;
	}
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
	getvariable('conn'), 
	'
		SELECT version()
	'
	)
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	std::cout << res.Value<std::string>(0, 0) << std::endl;
}

TEST_CASE("MySQL version query", group_name) {
	if (!(DBMSConfigured("MySQL") || DBMSConfigured("MariaDB"))) {
		return;
	}
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
	getvariable('conn'), 
	'
		SELECT version()
	'
	)
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	std::cout << res.Value<std::string>(0, 0) << std::endl;
}
