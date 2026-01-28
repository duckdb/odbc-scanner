#include "test_common.hpp"

static const std::string group_name = "[capi_basic]";

TEST_CASE("Basic query with a single value", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn,
	                               (R"(
SELECT * FROM odbc_query(
	getvariable('conn'), 
	'
		SELECT )" + CastAsBigintSQL("42") +
	                                R"(
	'
	)
)")
	                                   .c_str(),
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<int64_t>(0, 0) == 42);
}

TEST_CASE("Basic query with multiple rows and columns", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn,
	                               (R"(
SELECT * FROM odbc_query(
	getvariable('conn'),
	'
		SELECT a.col1, a.col2 FROM (
			SELECT ''foo'' AS col1, )" +
	                                CastAsBigintSQL("41", "AS col2") +
	                                R"(
			UNION ALL
			SELECT ''bar'' AS col1, )" +
	                                CastAsBigintSQL("42", "AS col2") +
	                                R"(
		) a
		ORDER BY a.col2
	')
)")
	                                   .c_str(),
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<std::string>(0, 0) == "foo");
	REQUIRE(res.Value<int64_t>(1, 0) == 41);
	REQUIRE(res.Value<std::string>(0, 1) == "bar");
	REQUIRE(res.Value<int64_t>(1, 1) == 42);
}

TEST_CASE("Basic query with a prepared statement", group_name) {
	ScannerConn sc;
	duckdb_prepared_statement ps_ptr = nullptr;
	duckdb_state st_prepare = duckdb_prepare(sc.conn,
	                                         (R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
		SELECT )" + CastAsBigintSQL("42") + R"(
  ')
)")
	                                             .c_str(),
	                                         &ps_ptr);
	REQUIRE(PreparedSuccess(ps_ptr, st_prepare));
	auto ps = PreparedStatementPtr(ps_ptr, PreparedStatementDeleter);

	{
		Result res;
		duckdb_state st_exec = duckdb_execute_prepared(ps.get(), res.Get());
		REQUIRE(QuerySuccess(res.Get(), st_exec));
		REQUIRE(res.NextChunk());
		REQUIRE(res.Value<int64_t>(0, 0) == 42);
	}

	if (DBMSConfigured("Spark")) {
		return;
	}

	{
		Result res;
		duckdb_state st_exec = duckdb_execute_prepared(ps.get(), res.Get());
		REQUIRE(QuerySuccess(res.Get(), st_exec));
		REQUIRE(res.NextChunk());
		REQUIRE(res.Value<int64_t>(0, 0) == 42);
	}
}

TEST_CASE("Update query with zero rows changed", group_name) {
	if (!(DBMSConfigured("DuckDB") || DBMSConfigured("MSSQL") || DBMSConfigured("PostgreSQL") ||
	      DBMSConfigured("Oracle") || DBMSConfigured("DB2") || DBMSConfigured("FlightSQL"))) {
		return;
	}

	ScannerConn sc;
	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn,
		                               (R"(
	SELECT * FROM odbc_query(
		getvariable('conn'), 
		'
			DROP TABLE )" + IfExistsSQL() +
		                                R"( duckdb_test_update_zero_rows
		',
		ignore_exec_failure=TRUE
		)
	)")
		                                   .c_str(),
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
	}
	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn,
		                               R"(
	SELECT * FROM odbc_query(
		getvariable('conn'), 
		'
			CREATE TABLE duckdb_test_update_zero_rows (col1 INTEGER)
		'
		)
	)",
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
	}
	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn,
		                               R"(
	SELECT * FROM odbc_query(
		getvariable('conn'), 
		'
			INSERT INTO duckdb_test_update_zero_rows VALUES(42)
		'
		)
	)",
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
	}
	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn,
		                               R"(
	SELECT * FROM odbc_query(
		getvariable('conn'), 
		'
			UPDATE duckdb_test_update_zero_rows SET col1 = 42 WHERE 1 < 0
		'
		)
	)",
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
	}
	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn,
		                               R"(
	SELECT * FROM odbc_query(
		getvariable('conn'), 
		'
			DROP TABLE duckdb_test_update_zero_rows
		'
		)
	)",
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
	}
}
