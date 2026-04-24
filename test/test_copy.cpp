#include "test_common.hpp"

#include <fstream>
#include <sstream>

static const std::string group_name = "[copy]";

TEST_CASE("Copy from file into Oracle", group_name) {
	if (!DBMSConfigured("Oracle")) {
		return;
	}

	std::string project_dir = ProjectRootDir();

	std::ifstream fstream(project_dir + "/resources/data/nl_stations_ora.sql", std::ios::in | std::ios::binary);
	fstream.exceptions(std::ios_base::failbit | std::ios_base::badbit);
	std::stringstream sstream;
	sstream << fstream.rdbuf();
	std::string ddl = sstream.str();

	ScannerConn sc;
	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn, R"(
  SELECT * FROM odbc_query(getvariable('conn'), 'DROP TABLE NL_TRAIN_STATIONS', ignore_exec_failure=TRUE)
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
		duckdb_state st = duckdb_query(sc.conn,
		                               (R"(
  SELECT * FROM odbc_copy(getvariable('conn'), dest_table='NL_TRAIN_STATIONS', source_file=')" +
		                                project_dir + R"(/resources/data/nl_stations_short.csv')
  )")
		                                   .c_str(),
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
	}
	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn, R"(
  SELECT * FROM odbc_query(getvariable('conn'), 'SELECT count(*) FROM NL_TRAIN_STATIONS')
  )",
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
		REQUIRE(res.NextChunk());
		REQUIRE(res.Value<double>(0, 0) == 3);
	}
	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn, R"(
  SELECT * FROM odbc_query(getvariable('conn'), 'SELECT "id", "code", "geo_lng" FROM NL_TRAIN_STATIONS WHERE "id" = 227')
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
  SELECT * FROM odbc_query(getvariable('conn'), 'DROP TABLE NL_TRAIN_STATIONS')
  )",
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
	}
}

TEST_CASE("Copy from file into Oracle with table creation", group_name) {
	if (!DBMSConfigured("Oracle")) {
		return;
	}

	std::string project_dir = ProjectRootDir();

	ScannerConn sc;
	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn, R"(
  SELECT * FROM odbc_query(getvariable('conn'), 'DROP TABLE NL_TRAIN_STATIONS', ignore_exec_failure=TRUE)
  )",
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
	}
	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn,
		                               (R"(
  SELECT * FROM odbc_copy(getvariable('conn'),
		dest_table='NL_TRAIN_STATIONS', source_file=')" +
		                                project_dir +
		                                R"(/resources/data/nl_stations_short.csv',
		create_table=TRUE)
  )")
		                                   .c_str(),
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
	}
	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn, R"(
  SELECT * FROM odbc_query(getvariable('conn'), 'SELECT count(*) FROM NL_TRAIN_STATIONS')
  )",
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
		REQUIRE(res.NextChunk());
		REQUIRE(res.Value<double>(0, 0) == 3);
	}
	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn, R"(
  SELECT * FROM odbc_query(getvariable('conn'), 'SELECT "id", "code", "geo_lng" FROM NL_TRAIN_STATIONS WHERE "id" = 227')
  )",
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
		REQUIRE(res.NextChunk());
		REQUIRE(res.DecimalValue<int64_t>(0, 0) == 227);
		REQUIRE(res.Value<std::string>(1, 0) == "HDE");
		REQUIRE(res.Value<double>(2, 0) == 5.893611);
	}
	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn, R"(
  SELECT * FROM odbc_query(getvariable('conn'), 'DROP TABLE NL_TRAIN_STATIONS')
  )",
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
	}
}

// Regression test for duckdb/odbc-scanner#161 / FirebirdSQL/firebird-odbc-driver#292:
// copying integer source rows into a VARCHAR primary-key column hit the ODBC driver's
// least-exercised numeric-C → character-SQL code path and caused silent data loss on
// the Firebird driver. The bug was silent from ODBC's perspective — every call
// returned SUCCESS — so a round-trip test that reads the stored VARCHAR back and
// compares to the source integer as a string is needed to surface regressions.
// batch_size=1 forces the single-row bind path (SetExpectedTypes + BindToOdbc per
// row) which is the shape that exposed the original bug.
TEST_CASE("Copy integer source into VARCHAR primary key", group_name) {
	// Skip:
	//  - NoSQL-ish / cloud-only drivers (no ad-hoc DDL): FlightSQL, Spark, Snowflake, ClickHouse.
	//  - MySQL / MariaDB: CI connection string does not select a default database, so
	//    `CREATE TABLE <name>` fails outright with "No database selected". The scanner
	//    bug this test guards against is not MySQL-specific and the other drivers
	//    cover it, so adding a database-selection dance just for this test is not
	//    worth the maintenance cost.
	if (DBMSConfigured("FlightSQL") || DBMSConfigured("Spark") || DBMSConfigured("Snowflake") ||
	    DBMSConfigured("ClickHouse") || DBMSConfigured("MySQL") || DBMSConfigured("MariaDB")) {
		return;
	}

	const std::string table_name = "int_to_varchar_pk_test";

	std::string varchar_type = "VARCHAR(20)";
	if (DBMSConfigured("Oracle")) {
		varchar_type = "VARCHAR2(20)";
	}

	// ignore_exec_failure only rescues SQLExecute failures; DuckDB's ODBC driver
	// (and some others) reject DROP TABLE <missing> at SQLPrepare. Drivers that
	// do accept "IF EXISTS" syntax take that branch; the rest keep the classic
	// ignore-on-exec path.
	bool supports_if_exists = DBMSConfigured("DuckDB") || DBMSConfigured("PostgreSQL") || DBMSConfigured("MSSQL");

	ScannerConn sc;
	{
		Result res;
		std::string drop_sql = supports_if_exists ? "DROP TABLE IF EXISTS " + table_name : "DROP TABLE " + table_name;
		std::string ignore_opt = supports_if_exists ? "" : ", ignore_exec_failure=TRUE";
		std::string sql = "SELECT * FROM odbc_query(getvariable('conn'), '" + drop_sql + "'" + ignore_opt + ")";
		duckdb_state st = duckdb_query(sc.conn, sql.c_str(), res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
	}
	{
		Result res;
		std::string sql = "SELECT * FROM odbc_query(getvariable('conn'), 'CREATE TABLE " + table_name + " (id " +
		                  varchar_type + " NOT NULL PRIMARY KEY)')";
		duckdb_state st = duckdb_query(sc.conn, sql.c_str(), res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
	}

	// Copy 500 integers into the VARCHAR PK column with batch_size=1 to force
	// the per-row rebind shape that exposed the Firebird silent-corruption bug.
	// column_quotes='' keeps the INSERT column names unquoted so drivers with
	// case-sensitive quoted identifiers (Oracle, DB2, Firebird — which fold
	// unquoted names to uppercase but keep quoted names case-sensitive) still
	// match the "id" column created above.
	// rows_processed comes back via the DuckDB side (always BIGINT), so the row-count
	// assertion does not depend on per-driver DECIMAL/NUMBER coercion — it catches
	// "row loss" regressions where dedup / UPDATE-OR-INSERT MATCHING collapses
	// distinct PKs.
	{
		Result res;
		// rows_processed is UBIGINT; sum() collapses any multi-chunk streaming into
		// a single scalar, and ::DECIMAL(18, 0) lines the column up with
		// Result::DecimalValue<int64_t>, which is the one int64 accessor that works
		// uniformly across all drivers here (Result::Value<int64_t> routes through
		// driver-specific type expectations).
		std::string sql = std::string() +
		                  "SELECT sum(rows_processed)::DECIMAL(18, 0) FROM odbc_copy(getvariable('conn'),\n"
		                  "  dest_table='" +
		                  table_name +
		                  "',\n"
		                  "  batch_size=1,\n"
		                  "  column_quotes='',\n"
		                  "  source_query='SELECT i::INTEGER AS id FROM range(1, 501) t(i)')";
		duckdb_state st = duckdb_query(sc.conn, sql.c_str(), res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
		REQUIRE(res.NextChunk());
		REQUIRE(res.DecimalValue<int64_t>(0, 0) == 500);
	}

	// The critical assertion: stored VARCHAR values must equal their source
	// integers as decimal strings. Row-count alone was not enough — the
	// Firebird bug stored 500 *corrupted* (NUL-byte) values without any driver
	// error for one execute shape, and collapsed to 11 distinct values for
	// another. Check a sample of small, multi-digit, and boundary values.
	{
		Result res;
		std::string inner_sql =
		    std::string() + "SELECT id FROM " + table_name + " WHERE id IN (''1'', ''2'', ''42'', ''500'') ORDER BY id";
		std::string sql = std::string() + "SELECT id FROM odbc_query(getvariable('conn'), '" + inner_sql + "')";
		duckdb_state st = duckdb_query(sc.conn, sql.c_str(), res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
		REQUIRE(res.NextChunk());
		// Lexicographic sort: '1','2','42','500'.
		REQUIRE(res.Value<std::string>(0, 0) == "1");
		REQUIRE(res.Value<std::string>(0, 1) == "2");
		REQUIRE(res.Value<std::string>(0, 2) == "42");
		REQUIRE(res.Value<std::string>(0, 3) == "500");
	}

	{
		Result res;
		std::string drop_sql = supports_if_exists ? "DROP TABLE IF EXISTS " + table_name : "DROP TABLE " + table_name;
		std::string sql = "SELECT * FROM odbc_query(getvariable('conn'), '" + drop_sql + "')";
		duckdb_state st = duckdb_query(sc.conn, sql.c_str(), res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
	}
}
