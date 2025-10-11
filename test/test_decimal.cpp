#include "test_common.hpp"

static const std::string group_name = "[capi_decimal]";

TEST_CASE("Decimal INT16 query with a literal", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(''1.234'' AS DECIMAL(4,3))
  ')
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int16_t>(0, 0) == 1234);
}

TEST_CASE("Decimal INT16 query with a negative literal", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(''-1.234'' AS DECIMAL(4,3))
  ')
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int16_t>(0, 0) == -1234);
}

TEST_CASE("Decimal INT32 query with a literal", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(''123456.789'' AS DECIMAL(9,3))
  ')
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int32_t>(0, 0) == 123456789);
}

TEST_CASE("Decimal INT32 query with a negative literal", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(''-123456.789'' AS DECIMAL(9,3))
  ')
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int32_t>(0, 0) == -123456789);
}

TEST_CASE("Decimal INT64 query with a literal", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(''123456789012345.123'' AS DECIMAL(18,3))
  ')
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int64_t>(0, 0) == 123456789012345123LL);
}

TEST_CASE("Decimal INT64 query with a negative literal", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(''-123456789012345.123'' AS DECIMAL(18,3))
  ')
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int64_t>(0, 0) == -123456789012345123LL);
}

TEST_CASE("Decimal INT128 query with a literal", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(''12345678901234567890123456789012345.123'' AS DECIMAL(38,3))
  ')
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).lower == 14143994781733810467ULL);
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).upper == 669260594276348691ULL);
}

/* TODO: checkme
TEST_CASE("Decimal INT128 query with a negative 1 literal", group_name) {
    ScannerConn sc;
    Result res;
    duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(''-1'' AS DECIMAL(38,3))
  ')
)",
                                   res.Get());
    REQUIRE(QuerySuccess(res.Get(), st));
    REQUIRE(res.NextChunk());
    REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).lower == 18446744073709551615ULL);
    REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).upper == -1LL);
}
*/

TEST_CASE("Decimal INT128 query with a negative literal", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(''-12345678901234567890123456789012345.123'' AS DECIMAL(38,3))
  ')
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).lower == 4302749291975741149ULL);
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).upper == -669260594276348692LL);
}

TEST_CASE("Decimal INT16 query with a literal parameter", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(? AS DECIMAL(4,3))
  ',
  params=row('1.234'::DECIMAL(4,3)))
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int16_t>(0, 0) == 1234);
}

TEST_CASE("Decimal INT16 query with a negative literal parameter", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(? AS DECIMAL(4,3))
  ',
  params=row('-1.234'::DECIMAL(4,3)))
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int16_t>(0, 0) == -1234);
}

TEST_CASE("Decimal INT128 query with a negative literal parameter", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(? AS DECIMAL(38,3))
  ',
  params=row('-12345678901234567890123456789012345.123'::DECIMAL(38,3)))
)",
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).lower == 4302749291975741149ULL);
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).upper == -669260594276348692LL);
}

TEST_CASE("Decimal INT16 query with a negative parameter", group_name) {
	ScannerConn sc;

	Result res_create_params;
	duckdb_state st_create_params = duckdb_query(sc.conn, R"(
SET VARIABLE params1 = odbc_create_params()
)",
	                                             res_create_params.Get());
	REQUIRE(QuerySuccess(res_create_params.Get(), st_create_params));

	duckdb_prepared_statement ps_ptr = nullptr;
	duckdb_state st_prepare = duckdb_prepare(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(? AS DECIMAL(4,3))
  ', 
  params_handle=getvariable('params1'))
)",
	                                         &ps_ptr);
	REQUIRE(PreparedSuccess(ps_ptr, st_prepare));
	auto ps = PreparedStatementPtr(ps_ptr, PreparedStatementDeleter);

	Result res_bind;
	duckdb_state st_bind_params = duckdb_query(sc.conn, R"(
SELECT odbc_bind_params(getvariable('conn'), getvariable('params1'), row('-1.234'::DECIMAL(4,3)))
)",
	                                           res_bind.Get());
	REQUIRE(QuerySuccess(res_bind.Get(), st_bind_params));

	Result res;
	duckdb_state st_exec = duckdb_execute_prepared(ps.get(), res.Get());

	REQUIRE(QuerySuccess(res.Get(), st_exec));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int16_t>(0, 0) == -1234);
}

TEST_CASE("Decimal INT128 query with a negative parameter", group_name) {
	ScannerConn sc;

	Result res_create_params;
	duckdb_state st_create_params = duckdb_query(sc.conn, R"(
SET VARIABLE params1 = odbc_create_params()
)",
	                                             res_create_params.Get());
	REQUIRE(QuerySuccess(res_create_params.Get(), st_create_params));

	duckdb_prepared_statement ps_ptr = nullptr;
	duckdb_state st_prepare = duckdb_prepare(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(? AS DECIMAL(38,3))
  ', 
  params_handle=getvariable('params1'))
)",
	                                         &ps_ptr);
	REQUIRE(PreparedSuccess(ps_ptr, st_prepare));
	auto ps = PreparedStatementPtr(ps_ptr, PreparedStatementDeleter);

	Result res_bind;
	duckdb_state st_bind_params = duckdb_query(sc.conn, R"(
SELECT odbc_bind_params(getvariable('conn'), getvariable('params1'), row('-12345678901234567890123456789012345.123'::DECIMAL(38,3)))
)",
	                                           res_bind.Get());
	REQUIRE(QuerySuccess(res_bind.Get(), st_bind_params));

	Result res;
	duckdb_state st_exec = duckdb_execute_prepared(ps.get(), res.Get());

	REQUIRE(QuerySuccess(res.Get(), st_exec));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).lower == 4302749291975741149ULL);
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).upper == -669260594276348692LL);
}
