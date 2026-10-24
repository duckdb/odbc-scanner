#include "test_common.hpp"

static const std::string group_name = "[capi_params]";

TEST_CASE("Params query with a single param literal", group_name) {
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
    SELECT )" + CastAsBigintSQL("?", "AS col1") +
	                 R"(
  ', 
  params=row(42))
)")
	                    .c_str(),
	                res);
	REQUIRE(QuerySuccess(res, st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<int64_t>(0, 0) == 42);
}

TEST_CASE("Params query with a varchar param literal", group_name) {
	std::string cast = "CAST(? AS VARCHAR(16))";
	if (DBMSConfigured("MySQL")) {
		cast = "CAST(? AS CHAR(16))";
	} else if (DBMSConfigured("ClickHouse")) {
		cast = "CAST(? AS Nullable(VARCHAR)) AS col1";
	} else if (DBMSConfigured("Oracle")) {
		cast = "CAST(? AS VARCHAR2(16)) FROM dual";
	} else if (DBMSConfigured("DB2")) {
		cast = "CAST(? AS VARCHAR(16)) FROM sysibm.sysdummy1";
	} else if (DBMSConfigured("Firebird")) {
		cast = "CAST(? AS VARCHAR(16)) FROM rdb$database";
	} else if (DBMSConfigured("FlightSQL")) {
		return;
	}
	ScannerConn sc;
	Result res;
	bool st = Query(sc.conn,
	                (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + cast +
	                 R"(
  ', 
  params=row('foo'))
)")
	                    .c_str(),
	                res);
	REQUIRE(QuerySuccess(res, st));
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
	bool st = Query(sc.conn, R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ?::UTINYINT AS col1
  ', 
  params=row(255::UTINYINT))
)",
	                res);
	REQUIRE(QuerySuccess(res, st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.Value<uint8_t>(0, 0) == 255);
}

TEST_CASE("Params query NULL BIGINT parameter", group_name) {
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
    SELECT )" + CastAsBigintSQL("?", "AS col1") +
	                 R"(
  ', 
  params=row(NULL))
)")
	                    .c_str(),
	                res);
	REQUIRE(QuerySuccess(res, st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.IsNull(0, 0));
}

TEST_CASE("Params query with multiple params including NULL", group_name) {
	if (DBMSConfigured("Oracle") || DBMSConfigured("DB2") || DBMSConfigured("FlightSQL")) {
		return;
	}
	if (DBMSConfigured("Firebird")) {
		ScannerConn sc;
		Result res;
		bool st = Query(sc.conn,
		                R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(? AS BIGINT) AS c1, CAST(? AS BIGINT) AS c2 FROM rdb$database
  ', 
  params=row(NULL, 42))
)",
		                res);
		REQUIRE(QuerySuccess(res, st));
		REQUIRE(res.NextChunk());
		REQUIRE(res.IsNull(0, 0));
		REQUIRE(res.Value<int64_t>(1, 0) == 42);
		return;
	}
	ScannerConn sc;
	Result res;
	bool st = Query(sc.conn,
	                (R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + CastAsBigintSQL("?", "AS c1") +
	                 ", " + CastAsBigintSQL("?", "AS c2") + R"(
  ', 
  params=row(NULL, 42))
)")
	                    .c_str(),
	                res);
	REQUIRE(QuerySuccess(res, st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.IsNull(0, 0));
	REQUIRE(res.Value<int64_t>(1, 0) == 42);
}

TEST_CASE("Params query Oracle with multiple params including NULL", group_name) {
	if (!DBMSConfigured("Oracle")) {
		return;
	}
	ScannerConn sc;
	Result res;
	bool st = Query(sc.conn,
	                R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(? AS NUMBER(18)) AS c1, CAST(? AS NUMBER(18)) AS c2 FROM dual
  ', 
  params=row(NULL, 42))
)",
	                res);
	REQUIRE(QuerySuccess(res, st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.IsNull(0, 0));
	REQUIRE(res.Value<int64_t>(1, 0) == 42);
}

TEST_CASE("Params query DB2 with multiple params including NULL", group_name) {
	if (!DBMSConfigured("DB2")) {
		return;
	}
	ScannerConn sc;
	Result res;
	bool st = Query(sc.conn,
	                R"(
FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(? AS BIGINT) AS c1, CAST(? AS BIGINT) AS c2 FROM sysibm.sysdummy1
  ', 
  params=row(NULL, 42))
)",
	                res);
	REQUIRE(QuerySuccess(res, st));
	REQUIRE(res.NextChunk());
	REQUIRE(res.IsNull(0, 0));
	REQUIRE(res.Value<int64_t>(1, 0) == 42);
}

TEST_CASE("Params query with rebinding", group_name) {
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
    SELECT )" + CastAsBigintSQL("?", "AS col1") +
	                           R"(
  ', 
  params=row(?))
)")
	                              .c_str(),
	                          ps);
	REQUIRE(PreparedSuccess(ps, st_prepare));

	{
		auto param_val = CreateInt32(sc, 42);
		bool st_bind = ps.Bind(1, param_val.get());
		REQUIRE(PreparedSuccess(ps, st_bind));

		Result res;
		bool st_exec = ps.Execute(res);
		REQUIRE(QuerySuccess(res, st_exec));
		REQUIRE(res.NextChunk());
		REQUIRE(res.Value<int64_t>(0, 0) == 42);
	}

	{
		auto param_val = CreateInt32(sc, 43);
		bool st_bind = ps.Bind(1, param_val.get());
		REQUIRE(PreparedSuccess(ps, st_bind));

		Result res;
		bool st_exec = ps.Execute(res);
		REQUIRE(QuerySuccess(res, st_exec));
		REQUIRE(res.NextChunk());
		REQUIRE(res.Value<int64_t>(0, 0) == 43);
	}
}
