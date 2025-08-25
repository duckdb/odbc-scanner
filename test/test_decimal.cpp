#include "test_common.hpp"

TEST_CASE("Decimal INT16 query with a literal", "[capi_decimal]") {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ''1.234''::DECIMAL(4,3)
  ')
)",
	                               res.Get());
	REQUIRE(st == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int16_t>(0, 0) == 1234);
}

TEST_CASE("Decimal INT16 query with a negative literal", "[capi_decimal]") {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ''-1.234''::DECIMAL(4,3)
  ')
)",
	                               res.Get());
	REQUIRE(st == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int16_t>(0, 0) == -1234);
}

TEST_CASE("Decimal INT32 query with a literal", "[capi_decimal]") {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ''123456.789''::DECIMAL(9,3)
  ')
)",
	                               res.Get());
	REQUIRE(st == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int32_t>(0, 0) == 123456789);
}

TEST_CASE("Decimal INT32 query with a negative literal", "[capi_decimal]") {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ''-123456.789''::DECIMAL(9,3)
  ')
)",
	                               res.Get());
	REQUIRE(st == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int32_t>(0, 0) == -123456789);
}

TEST_CASE("Decimal INT64 query with a literal", "[capi_decimal]") {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ''123456789012345.123''::DECIMAL(18,3)
  ')
)",
	                               res.Get());
	REQUIRE(st == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int64_t>(0, 0) == 123456789012345123LL);
}

TEST_CASE("Decimal INT64 query with a negative literal", "[capi_decimal]") {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ''-123456789012345.123''::DECIMAL(18,3)
  ')
)",
	                               res.Get());
	REQUIRE(st == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int64_t>(0, 0) == -123456789012345123LL);
}

TEST_CASE("Decimal INT128 query with a literal", "[capi_decimal]") {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ''12345678901234567890123456789012345.123''::DECIMAL(38,3)
  ')
)",
	                               res.Get());
	REQUIRE(st == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).lower == 14143994781733810467ULL);
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).upper == 669260594276348691ULL);
}

TEST_CASE("Decimal INT128 query with a negative 1 literal", "[capi_decimal]") {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ''-1''::DECIMAL(38,3)
  ')
)",
	                               res.Get());
	REQUIRE(st == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).lower == 18446744073709551615ULL);
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).upper == -1LL);
}

TEST_CASE("Decimal INT128 query with a negative literal", "[capi_decimal]") {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ''-12345678901234567890123456789012345.123''::DECIMAL(38,3)
  ')
)",
	                               res.Get());
	REQUIRE(st == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).lower == 4302749291975741149ULL);
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).upper == -669260594276348692LL);
}

TEST_CASE("Decimal INT16 query with a literal parameter", "[capi_decimal]") {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ?::DECIMAL(4,3)
  ',
  params=row('1.234'::DECIMAL))
)",
	                               res.Get());
	REQUIRE(st == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int16_t>(0, 0) == 1234);
}

TEST_CASE("Decimal INT16 query with a negative literal parameter", "[capi_decimal]") {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ?::DECIMAL(4,3)
  ',
  params=row('-1.234'::DECIMAL))
)",
	                               res.Get());
	REQUIRE(st == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int16_t>(0, 0) == -1234);
}

TEST_CASE("Decimal INT128 query with a negative literal parameter", "[capi_decimal]") {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ?::DECIMAL(38,3)
  ',
  params=row('-12345678901234567890123456789012345.123'::DECIMAL(38,3)))
)",
	                               res.Get());
	REQUIRE(st == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).lower == 4302749291975741149ULL);
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).upper == -669260594276348692LL);
}

TEST_CASE("Decimal INT16 query with a negative parameter", "[capi_decimal]") {
	ScannerConn sc;

	duckdb_state st_create_params = duckdb_query(sc.conn, R"(
SET VARIABLE params1 = odbc_create_params()
)",
	                                             nullptr);
	REQUIRE(st_create_params == DuckDBSuccess);

	duckdb_prepared_statement ps_ptr = nullptr;
	duckdb_state st_prepare = duckdb_prepare(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ?::DECIMAL(4,3)
  ', 
  params_handle=getvariable('params1'))
)",
	                                         &ps_ptr);
	REQUIRE(st_prepare == DuckDBSuccess);
	auto ps = PreparedStatementPtr(ps_ptr, PreparedStatementDeleter);

	duckdb_state st_bind_params = duckdb_query(sc.conn, R"(
SELECT odbc_bind_params(getvariable('params1'), row('-1.234'::DECIMAL))
)",
	                                           nullptr);
	REQUIRE(st_bind_params == DuckDBSuccess);

	Result res;
	duckdb_state st_exec = duckdb_execute_prepared(ps.get(), res.Get());

	REQUIRE(st_exec == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int16_t>(0, 0) == -1234);
}

TEST_CASE("Decimal INT128 query with a negative parameter", "[capi_decimal]") {
	ScannerConn sc;

	duckdb_state st_create_params = duckdb_query(sc.conn, R"(
SET VARIABLE params1 = odbc_create_params()
)",
	                                             nullptr);
	REQUIRE(st_create_params == DuckDBSuccess);

	duckdb_prepared_statement ps_ptr = nullptr;
	duckdb_state st_prepare = duckdb_prepare(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ?::DECIMAL(38,3)
  ', 
  params_handle=getvariable('params1'))
)",
	                                         &ps_ptr);
	REQUIRE(st_prepare == DuckDBSuccess);
	auto ps = PreparedStatementPtr(ps_ptr, PreparedStatementDeleter);

	duckdb_state st_bind_params = duckdb_query(sc.conn, R"(
SELECT odbc_bind_params(getvariable('params1'), row('-12345678901234567890123456789012345.123'::DECIMAL(38,3)))
)",
	                                           nullptr);
	REQUIRE(st_bind_params == DuckDBSuccess);

	Result res;
	duckdb_state st_exec = duckdb_execute_prepared(ps.get(), res.Get());

	REQUIRE(st_exec == DuckDBSuccess);
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).lower == 4302749291975741149ULL);
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).upper == -669260594276348692LL);
}
