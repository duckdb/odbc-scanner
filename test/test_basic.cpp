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
	                                CastAsBigintSQL("41") +
	                                R"( AS col2
			UNION ALL
			SELECT ''bar'' AS col1, )" +
	                                CastAsBigintSQL("42") +
	                                R"( AS col2
		) AS a
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
