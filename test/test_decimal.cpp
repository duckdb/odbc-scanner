#include "test_common.hpp"

static const std::string group_name = "[capi_decimal]";

TEST_CASE("Decimal INT16 query with a literal", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn,
	                               (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDecimalSQL("''1.234''", 4, 3) +
	                                R"(
  ')
)")
	                                   .c_str(),
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int16_t>(0, 0) == 1234);
}

TEST_CASE("Decimal INT16 query with a negative literal", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn,
	                               (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDecimalSQL("''-1.234''", 4, 3) +
	                                R"(
  ')
)")
	                                   .c_str(),
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int16_t>(0, 0) == -1234);
}

TEST_CASE("Decimal INT32 query with a literal", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn,
	                               (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDecimalSQL("''123456.789''", 9, 3) +
	                                R"(
  ')
)")
	                                   .c_str(),
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int32_t>(0, 0) == 123456789);
}

TEST_CASE("Decimal INT32 query with a negative literal", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn,
	                               (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDecimalSQL("''-123456.789''", 9, 3) +
	                                R"(
  ')
)")
	                                   .c_str(),
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int32_t>(0, 0) == -123456789);
}

TEST_CASE("Decimal INT64 query with a literal", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn,
	                               (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDecimalSQL("''123456789012345.123''", 18, 3) +
	                                R"(
  ')
)")
	                                   .c_str(),
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int64_t>(0, 0) == 123456789012345123LL);
}

TEST_CASE("Decimal INT64 query with a negative literal", group_name) {
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn,
	                               (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDecimalSQL("''-123456789012345.123''", 18, 3) +
	                                R"(
  ')
)")
	                                   .c_str(),
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int64_t>(0, 0) == -123456789012345123LL);
}

TEST_CASE("Decimal INT128 query with a literal", group_name) {
	if (DBMSConfigured("DB2") || DBMSConfigured("Firebird") || DBMSConfigured("Informix")) {
		return;
	}
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn,
	                               (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDecimalSQL("''12345678901234567890123456789012345.123''", 38, 3) +
	                                R"(
  ')
)")
	                                   .c_str(),
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
FROM odbc_query(
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
	if (DBMSConfigured("DB2") || DBMSConfigured("Firebird") || DBMSConfigured("Informix")) {
		return;
	}
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn,
	                               (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDecimalSQL("''-12345678901234567890123456789012345.123''", 38, 3) +
	                                R"(
  ')
)")
	                                   .c_str(),
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).lower == 4302749291975741149ULL);
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).upper == -669260594276348692LL);
}

TEST_CASE("Decimal INT16 query with a literal parameter", group_name) {
	if (DBMSConfigured("FlightSQL")) {
		return;
	}
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn,
	                               (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDecimalSQL("?", 4, 3, "AS col1") +
	                                R"(
  ',
  params=row('1.234'::DECIMAL(4,3)))
)")
	                                   .c_str(),
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int16_t>(0, 0) == 1234);
}

TEST_CASE("Decimal INT16 query with a negative literal parameter", group_name) {
	if (DBMSConfigured("FlightSQL")) {
		return;
	}
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn,
	                               (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDecimalSQL("?", 4, 3, "AS col1") +
	                                R"(
  ',
  params=row('-1.234'::DECIMAL(4,3)))
)")
	                                   .c_str(),
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int16_t>(0, 0) == -1234);
}

TEST_CASE("Decimal INT128 query with a negative literal parameter", group_name) {
	if (DBMSConfigured("DB2") || DBMSConfigured("FlightSQL") || DBMSConfigured("Firebird") ||
	    DBMSConfigured("Informix")) {
		return;
	}
	ScannerConn sc;
	Result res;
	duckdb_state st = duckdb_query(sc.conn,
	                               (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDecimalSQL("?", 38, 3, "AS col1") +
	                                R"(
  ',
  params=row('-12345678901234567890123456789012345.123'::DECIMAL(38,3)))
)")
	                                   .c_str(),
	                               res.Get());
	REQUIRE(QuerySuccess(res.Get(), st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).lower == 4302749291975741149ULL);
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).upper == -669260594276348692LL);
}

TEST_CASE("Decimal INT16 query with a negative parameter", group_name) {
	if (DBMSConfigured("FlightSQL")) {
		return;
	}
	ScannerConn sc;

	duckdb_prepared_statement ps_ptr = nullptr;
	duckdb_state st_prepare = duckdb_prepare(sc.conn,
	                                         (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDecimalSQL("?", 4, 3, "AS col1") +
	                                          R"(
  ', 
  params=row(?))
)")
	                                             .c_str(),
	                                         &ps_ptr);
	REQUIRE(PreparedSuccess(ps_ptr, st_prepare));
	auto ps = PreparedStatementPtr(ps_ptr, PreparedStatementDeleter);

	duckdb_hugeint param_hugeint = {static_cast<uint64_t>(-1234LL), -1LL};
	duckdb_decimal param_dec = {4, 3, param_hugeint};
	auto param_val = ValuePtr(duckdb_create_decimal(param_dec), ValueDeleter);
	duckdb_state st_bind = duckdb_bind_value(ps.get(), 1, param_val.get());
	REQUIRE(PreparedSuccess(ps_ptr, st_bind));

	Result res;
	duckdb_state st_exec = duckdb_execute_prepared(ps.get(), res.Get());

	REQUIRE(QuerySuccess(res.Get(), st_exec));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int16_t>(0, 0) == -1234);
}

TEST_CASE("Decimal INT128 query with a negative parameter", group_name) {
	if (DBMSConfigured("DB2") || DBMSConfigured("FlightSQL") || DBMSConfigured("Firebird") ||
	    DBMSConfigured("Informix")) {
		return;
	}
	ScannerConn sc;

	duckdb_prepared_statement ps_ptr = nullptr;
	duckdb_state st_prepare = duckdb_prepare(sc.conn,
	                                         (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDecimalSQL("?", 38, 3, "AS col1") +
	                                          R"(
  ', 
  params=row(?))
)")
	                                             .c_str(),
	                                         &ps_ptr);
	REQUIRE(PreparedSuccess(ps_ptr, st_prepare));
	auto ps = PreparedStatementPtr(ps_ptr, PreparedStatementDeleter);

	// -12345678901234567890123456789012345.123
	duckdb_hugeint param_hugeint = {4302749291975741149ULL, -669260594276348692LL};
	duckdb_decimal param_dec = {38, 3, param_hugeint};
	auto param_val = ValuePtr(duckdb_create_decimal(param_dec), ValueDeleter);
	duckdb_state st_bind = duckdb_bind_value(ps.get(), 1, param_val.get());
	REQUIRE(PreparedSuccess(ps_ptr, st_bind));

	Result res;
	duckdb_state st_exec = duckdb_execute_prepared(ps.get(), res.Get());

	REQUIRE(QuerySuccess(res.Get(), st_exec));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).lower == 4302749291975741149ULL);
	REQUIRE(res.DecimalValue<duckdb_hugeint>(0, 0).upper == -669260594276348692LL);
}
