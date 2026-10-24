#include "test_common.hpp"

static const std::string group_name = "[capi_decimal]";

TEST_CASE("Decimal INT16 query with a literal", group_name) {
	ScannerConn sc;
	Result res;
	bool st = Query(sc.conn,
	                (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDecimalSQL("''1.234''", 4, 3) +
	                 R"(
  ')
)")
	                    .c_str(),
	                res);
	REQUIRE(QuerySuccess(res, st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int16_t>(0, 0) == 1234);
}

TEST_CASE("Decimal INT16 query with a negative literal", group_name) {
	ScannerConn sc;
	Result res;
	bool st = Query(sc.conn,
	                (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDecimalSQL("''-1.234''", 4, 3) +
	                 R"(
  ')
)")
	                    .c_str(),
	                res);
	REQUIRE(QuerySuccess(res, st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int16_t>(0, 0) == -1234);
}

TEST_CASE("Decimal INT32 query with a literal", group_name) {
	ScannerConn sc;
	Result res;
	bool st = Query(sc.conn,
	                (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDecimalSQL("''123456.789''", 9, 3) +
	                 R"(
  ')
)")
	                    .c_str(),
	                res);
	REQUIRE(QuerySuccess(res, st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int32_t>(0, 0) == 123456789);
}

TEST_CASE("Decimal INT32 query with a negative literal", group_name) {
	ScannerConn sc;
	Result res;
	bool st = Query(sc.conn,
	                (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDecimalSQL("''-123456.789''", 9, 3) +
	                 R"(
  ')
)")
	                    .c_str(),
	                res);
	REQUIRE(QuerySuccess(res, st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int32_t>(0, 0) == -123456789);
}

TEST_CASE("Decimal INT64 query with a literal", group_name) {
	ScannerConn sc;
	Result res;
	bool st = Query(sc.conn,
	                (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDecimalSQL("''123456789012345.123''", 18, 3) +
	                 R"(
  ')
)")
	                    .c_str(),
	                res);
	REQUIRE(QuerySuccess(res, st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int64_t>(0, 0) == 123456789012345123LL);
}

TEST_CASE("Decimal INT64 query with a negative literal", group_name) {
	ScannerConn sc;
	Result res;
	bool st = Query(sc.conn,
	                (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDecimalSQL("''-123456789012345.123''", 18, 3) +
	                 R"(
  ')
)")
	                    .c_str(),
	                res);
	REQUIRE(QuerySuccess(res, st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int64_t>(0, 0) == -123456789012345123LL);
}

TEST_CASE("Decimal INT128 query with a literal", group_name) {
	if (DBMSConfigured("DB2") || DBMSConfigured("Firebird") || DBMSConfigured("Informix")) {
		return;
	}
	ScannerConn sc;
	Result res;
	bool st = Query(sc.conn,
	                (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDecimalSQL("''12345678901234567890123456789012345.123''", 38, 3) +
	                 R"(
  ')
)")
	                    .c_str(),
	                res);
	REQUIRE(QuerySuccess(res, st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int128_t>(0, 0).lower == 14143994781733810467ULL);
	REQUIRE(res.DecimalValue<int128_t>(0, 0).upper == 669260594276348691ULL);
}

/* TODO: checkme
TEST_CASE("Decimal INT128 query with a negative 1 literal", group_name) {
    ScannerConn sc;
    Result res;
    bool st = Query(sc.conn, R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(''-1'' AS DECIMAL(38,3))
  ')
)", res);
    REQUIRE(QuerySuccess(res, st));
    REQUIRE(res.NextChunk());
    REQUIRE(res.DecimalValue<int128_t>(0, 0).lower == 18446744073709551615ULL);
    REQUIRE(res.DecimalValue<int128_t>(0, 0).upper == -1LL);
}
*/

TEST_CASE("Decimal INT128 query with a negative literal", group_name) {
	if (DBMSConfigured("DB2") || DBMSConfigured("Firebird") || DBMSConfigured("Informix")) {
		return;
	}
	ScannerConn sc;
	Result res;
	bool st = Query(sc.conn,
	                (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsDecimalSQL("''-12345678901234567890123456789012345.123''", 38, 3) +
	                 R"(
  ')
)")
	                    .c_str(),
	                res);
	REQUIRE(QuerySuccess(res, st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int128_t>(0, 0).lower == 4302749291975741149ULL);
	REQUIRE(res.DecimalValue<int128_t>(0, 0).upper == -669260594276348692LL);
}

TEST_CASE("Decimal INT16 query with a literal parameter", group_name) {
	if (DBMSConfigured("FlightSQL")) {
		return;
	}
	ScannerConn sc;
	Result res;
	bool st = Query(sc.conn,
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
	                res);
	REQUIRE(QuerySuccess(res, st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int16_t>(0, 0) == 1234);
}

TEST_CASE("Decimal INT16 query with a negative literal parameter", group_name) {
	if (DBMSConfigured("FlightSQL")) {
		return;
	}
	ScannerConn sc;
	Result res;
	bool st = Query(sc.conn,
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
	                res);
	REQUIRE(QuerySuccess(res, st));
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
	bool st = Query(sc.conn,
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
	                res);
	REQUIRE(QuerySuccess(res, st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int128_t>(0, 0).lower == 4302749291975741149ULL);
	REQUIRE(res.DecimalValue<int128_t>(0, 0).upper == -669260594276348692LL);
}

TEST_CASE("Decimal INT16 query with a negative parameter", group_name) {
	if (DBMSConfigured("FlightSQL")) {
		return;
	}
	ScannerConn sc;

	PreparedStatement ps;
	bool st_prepare = Prepare(sc.conn,
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
	                          ps);
	REQUIRE(PreparedSuccess(ps, st_prepare));

	int128_t param_hugeint = {static_cast<uint64_t>(-1234LL), -1LL};
	auto param_val = CreateDecimal(sc, param_hugeint, 4, 3);
	bool st_bind = ps.Bind(1, param_val.get());
	REQUIRE(PreparedSuccess(ps, st_bind));

	Result res;
	bool st_exec = ps.Execute(res);

	REQUIRE(QuerySuccess(res, st_exec));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int16_t>(0, 0) == -1234);
}

TEST_CASE("Decimal INT128 query with a negative parameter", group_name) {
	if (DBMSConfigured("DB2") || DBMSConfigured("FlightSQL") || DBMSConfigured("Firebird") ||
	    DBMSConfigured("Informix")) {
		return;
	}
	ScannerConn sc;

	PreparedStatement ps;
	bool st_prepare = Prepare(sc.conn,
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
	                          ps);
	REQUIRE(PreparedSuccess(ps, st_prepare));

	// -12345678901234567890123456789012345.123
	int128_t param_hugeint = {4302749291975741149ULL, -669260594276348692LL};
	auto param_val = CreateDecimal(sc, param_hugeint, 38, 3);
	bool st_bind = ps.Bind(1, param_val.get());
	REQUIRE(PreparedSuccess(ps, st_bind));

	Result res;
	bool st_exec = ps.Execute(res);

	REQUIRE(QuerySuccess(res, st_exec));
	REQUIRE(res.NextChunk());
	REQUIRE(res.DecimalValue<int128_t>(0, 0).lower == 4302749291975741149ULL);
	REQUIRE(res.DecimalValue<int128_t>(0, 0).upper == -669260594276348692LL);
}
