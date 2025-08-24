#include "test_common.hpp"

TEST_CASE("Params query with a single param literal", "[capi_params]") {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ?::INT
  ', 
  params=row(42))
)",
	                               res.Get());
	REQUIRE(st == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<int32_t>(0, 0) == 42);
}

TEST_CASE("Params query with a varchar param literal", "[capi_params]") {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ?::VARCHAR
  ', 
  params=row('foo'))
)",
	                               res.Get());
	REQUIRE(st == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<std::string>(0, 0) == "foo");
}

TEST_CASE("Params query with an unsigned integer param", "[capi_params]") {
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
	REQUIRE(st == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<uint8_t>(0, 0) == 255);
}

TEST_CASE("Params query with multiple params including NULL", "[capi_params]") {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT coalesce(?::INT, ?::INT)
  ', 
  params=row(NULL, 42))
)",
	                               res.Get());
	REQUIRE(st == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<int32_t>(0, 0) == 42);
}

TEST_CASE("Params query with rebinding", "[capi_params]") {
	ScannerConn sc;
	duckdb_prepared_statement ps_ptr = nullptr;
	duckdb_state st_prepare = duckdb_prepare(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ?::INT
  ', 
  params=row(?))
)",
	                                         &ps_ptr);
	REQUIRE(st_prepare == DuckDBSuccess);
	auto ps = PreparedStatementPtr(ps_ptr, PreparedStatementDeleter);

	{
		auto param_val = ValuePtr(duckdb_create_int32(42), ValueDeleter);
		duckdb_state st_bind = duckdb_bind_value(ps.get(), 1, param_val.get());
		REQUIRE(st_bind == DuckDBSuccess);

		Result res;
		duckdb_state st_exec = duckdb_execute_prepared(ps.get(), res.Get());
		REQUIRE(st_exec == DuckDBSuccess);
		REQUIRE(res.NextChunk());
		REQUIRE(res.Value<int32_t>(0, 0) == 42);
	}

	{
		auto param_val = ValuePtr(duckdb_create_int32(43), ValueDeleter);
		duckdb_state st_bind = duckdb_bind_value(ps.get(), 1, param_val.get());
		REQUIRE(st_bind == DuckDBSuccess);

		Result res;
		duckdb_state st_exec = duckdb_execute_prepared(ps.get(), res.Get());
		REQUIRE(st_exec == DuckDBSuccess);
		REQUIRE(res.NextChunk());
		REQUIRE(res.Value<int32_t>(0, 0) == 43);
	}
}

TEST_CASE("Params query without rebinding", "[capi_params]") {
	ScannerConn sc;

	{
		duckdb_state st = duckdb_query(sc.conn, R"(
SET VARIABLE params1 = odbc_create_params()
)",
		                               nullptr);
		REQUIRE(st == DuckDBSuccess);
	}

	duckdb_prepared_statement ps_ptr = nullptr;
	duckdb_state st_prepare = duckdb_prepare(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ?::INT
  ', 
  params_handle=getvariable('params1'))
)",
	                                         &ps_ptr);
	REQUIRE(st_prepare == DuckDBSuccess);
	auto ps = PreparedStatementPtr(ps_ptr, PreparedStatementDeleter);

	{
		duckdb_state st = duckdb_query(sc.conn, R"(
SELECT odbc_bind_params(getvariable('params1'), row(42))
)",
		                               nullptr);
		REQUIRE(st == DuckDBSuccess);
	}

	{
		Result res;
		duckdb_state st_exec = duckdb_execute_prepared(ps.get(), res.Get());
		REQUIRE(st_exec == DuckDBSuccess);
		REQUIRE(res.NextChunk());
		REQUIRE(res.Value<int32_t>(0, 0) == 42);
	}

	{
		duckdb_state st = duckdb_query(sc.conn, R"(
SELECT odbc_bind_params(getvariable('params1'), row(43))
)",
		                               nullptr);
		REQUIRE(st == DuckDBSuccess);
	}

	{
		Result res;
		duckdb_state st_exec = duckdb_execute_prepared(ps.get(), res.Get());
		REQUIRE(st_exec == DuckDBSuccess);
		REQUIRE(res.NextChunk());
		REQUIRE(res.Value<int32_t>(0, 0) == 43);
	}
}
