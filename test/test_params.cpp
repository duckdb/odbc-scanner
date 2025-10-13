#include "test_common.hpp"

static const std::string group_name = "[capi_params]";

TEST_CASE("Params query with a single param literal", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(? AS INTEGER)
  ', 
  params=row(42))
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<int32_t>(0, 0) == 42);
}

TEST_CASE("Params query with a varchar param literal", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(? AS VARCHAR(16))
  ', 
  params=row('foo'))
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	std::string a = res.Value<std::string>(0, 0);
	REQUIRE(res.Value<std::string>(0, 0) == "foo");
}

TEST_CASE("Params query with an unsigned integer param", group_name) {
	if (!DBMSConfigured("DuckDB")) {
		return;
	}
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ?::UTINYINT
  ', 
  params=row(255::UTINYINT))
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<uint8_t>(0, 0) == 255);
}

TEST_CASE("Params query NULL INTEGER parameter", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(? AS INTEGER)
  ', 
  params=row(NULL))
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.IsNull(0, 0));
}

TEST_CASE("Params query with multiple params including NULL", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT coalesce(CAST(? AS INTEGER), CAST(? AS INTEGER))
  ', 
  params=row(NULL, 42))
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<int32_t>(0, 0) == 42);
}

TEST_CASE("Params query with rebinding", group_name) {
	ScannerConn sc;
	duckdb_prepared_statement ps_ptr = nullptr;
	duckdb_state st_prepare = duckdb_prepare(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(? AS INTEGER)
  ', 
  params=row(?))
)",
	                                         &ps_ptr);
	REQUIRE(PreparedSuccess(ps_ptr, st_prepare));
	auto ps = PreparedStatementPtr(ps_ptr, PreparedStatementDeleter);

	{
		auto param_val = ValuePtr(duckdb_create_int32(42), ValueDeleter);
		duckdb_state st_bind = duckdb_bind_value(ps.get(), 1, param_val.get());
		REQUIRE(PreparedSuccess(ps_ptr, st_bind));

		Result res;
		duckdb_state st_exec = duckdb_execute_prepared(ps.get(), res.Get());
		REQUIRE(QuerySuccess(res.Get(), st_exec));
		REQUIRE(res.NextChunk());
		REQUIRE(res.Value<int32_t>(0, 0) == 42);
	}

	{
		auto param_val = ValuePtr(duckdb_create_int32(43), ValueDeleter);
		duckdb_state st_bind = duckdb_bind_value(ps.get(), 1, param_val.get());
		REQUIRE(PreparedSuccess(ps_ptr, st_bind));

		Result res;
		duckdb_state st_exec = duckdb_execute_prepared(ps.get(), res.Get());
		REQUIRE(QuerySuccess(res.Get(), st_exec));
		REQUIRE(res.NextChunk());
		// MySQL changes column type from INT to BIGINT on `SQLExecute`
		if (DBMSConfigured("MySQL") || DBMSConfigured("MariaDB")) {
			// seems to be inconsistent between versions
			// REQUIRE(res.Value<int64_t>(0, 0) == 43);
		} else {
			REQUIRE(res.Value<int32_t>(0, 0) == 43);
		}
	}
}

TEST_CASE("Params query without rebinding", group_name) {
	ScannerConn sc;

	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn, R"(
SET VARIABLE params1 = odbc_create_params()
)",
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
	}

	duckdb_prepared_statement ps_ptr = nullptr;
	duckdb_state st_prepare = duckdb_prepare(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(? AS INTEGER)
  ', 
  params_handle=getvariable('params1'))
)",
	                                         &ps_ptr);
	REQUIRE(PreparedSuccess(ps_ptr, st_prepare));
	auto ps = PreparedStatementPtr(ps_ptr, PreparedStatementDeleter);

	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn, R"(
SELECT odbc_bind_params(getvariable('conn'), getvariable('params1'), row(42))
)",
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
	}

	{
		Result res;
		duckdb_state st_exec = duckdb_execute_prepared(ps.get(), res.Get());
		REQUIRE(QuerySuccess(res.Get(), st_exec));
		REQUIRE(res.NextChunk());
		REQUIRE(res.Value<int32_t>(0, 0) == 42);
	}

	{
		Result res;
		duckdb_state st = duckdb_query(sc.conn, R"(
SELECT odbc_bind_params(getvariable('conn'), getvariable('params1'), row(43))
)",
		                               res.Get());
		REQUIRE(QuerySuccess(res.Get(), st));
	}

	{
		Result res;
		duckdb_state st_exec = duckdb_execute_prepared(ps.get(), res.Get());
		REQUIRE(QuerySuccess(res.Get(), st_exec));
		REQUIRE(res.NextChunk());
		REQUIRE(res.Value<int32_t>(0, 0) == 43);
	}
}
