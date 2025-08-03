#include <cstring>

#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "duckdb.h"

#define SCANNER_QUOTE(value)                 #value
#define SCANNER_STR(value)                   SCANNER_QUOTE(value)
#define ODBC_SCANNER_EXTENSION_FILE_PATH_STR SCANNER_STR(ODBC_SCANNER_EXTENSION_FILE_PATH)

TEST_CASE("Scanner basic test", "[basic]") {
	duckdb_database database;
	duckdb_connection connection;
	duckdb_config config = nullptr;

	REQUIRE(duckdb_create_config(&config) == DuckDBSuccess);
	REQUIRE(duckdb_set_config(config, "allow_unsigned_extensions", "true") == DuckDBSuccess);
	REQUIRE(duckdb_set_config(config, "threads", "1") == DuckDBSuccess);

	REQUIRE(duckdb_open_ext(NULL, &database, config, nullptr) == DuckDBSuccess);
	REQUIRE(duckdb_connect(database, &connection) == DuckDBSuccess);

	duckdb_result result;
	std::string load_query = "LOAD '" + std::string(ODBC_SCANNER_EXTENSION_FILE_PATH_STR) + "'";
	REQUIRE(duckdb_query(connection, load_query.c_str(), &result) == DuckDBSuccess);

	REQUIRE(duckdb_query(connection, "SET VARIABLE conn = odbc_connect('Driver={DuckDB Driver};threads=1;')",
	                     &result) == DuckDBSuccess);

	REQUIRE(duckdb_query(connection, "SELECT odbc_params(41, 42)", &result) == DuckDBSuccess);

	REQUIRE(duckdb_query(connection,
	                     "SELECT * FROM odbc_query(getvariable('conn'), 'SELECT ?::INTEGER, ?::INTEGER, ?::INTEGER', "
	                     "odbc_params(41, 42, 43))",
	                     &result) == DuckDBSuccess);
	REQUIRE(duckdb_value_int64(&result, 0, 0) == 41);
	REQUIRE(duckdb_value_int64(&result, 1, 0) == 42);
	REQUIRE(duckdb_value_int64(&result, 2, 0) == 43);

	REQUIRE(duckdb_query(connection, "SELECT * FROM odbc_query(getvariable('conn'), 'SELECT ''foo''', NULL)",
	                     &result) == DuckDBSuccess);
	REQUIRE(std::strcmp(duckdb_value_varchar(&result, 0, 0), "foo") == 0);

	REQUIRE(duckdb_query(connection, "SELECT odbc_close(getvariable('conn'))", &result) == DuckDBSuccess);

	duckdb_disconnect(&connection);
	duckdb_close(&database);
}
