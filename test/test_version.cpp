#include "test_common.hpp"

static const std::string group_name = "[capi_version]";

TEST_CASE("Common version query", group_name) {
	if (!(DBMSConfigured("DuckDB") || DBMSConfigured("PostgreSQL") || DBMSConfigured("MySQL") ||
	      DBMSConfigured("MariaDB") || DBMSConfigured("ClickHouse") || DBMSConfigured("Spark"))) {
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

TEST_CASE("Oracle version query", group_name) {
	if (!(DBMSConfigured("Oracle"))) {
		return;
	}
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
	getvariable('conn'), 
	'
		SELECT * FROM PRODUCT_COMPONENT_VERSION
	'
	)
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	std::cout << res.Value<std::string>(0, 0) << std::endl;
	// std::cout << res.Value<std::string>(1, 0) << std::endl;
	std::cout << res.Value<std::string>(2, 0) << std::endl;
	// std::cout << res.Value<std::string>(3, 0) << std::endl;
}

TEST_CASE("DB2 version query", group_name) {
	if (!(DBMSConfigured("DB2"))) {
		return;
	}
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
	getvariable('conn'), 
	'
		SELECT SERVICE_LEVEL FROM SYSIBMADM.ENV_INST_INFO
	'
	)
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	std::cout << res.Value<std::string>(0, 0) << std::endl;
}
