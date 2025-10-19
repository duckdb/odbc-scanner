#include "test_common.hpp"

#include <vector>

static const std::string group_name = "[capi_long_strings]";

static std::string GenStr(size_t len) {
	std::string res;
	for (size_t i = 0; i < len; i++) {
		std::string ch = std::to_string(i % 10);
		res.append(ch);
	}
	REQUIRE(res.length() == len);
	return res;
}

TEST_CASE("Long string query", group_name) {
	std::string cast = "NOT_SUPPORTED";
	if (DBMSConfigured("DuckDB")) {
		cast = "CAST(? AS VARCHAR)";
	} else if (DBMSConfigured("MSSQL")) {
		cast = "CAST(? AS VARCHAR(max))";
	} else if (DBMSConfigured("PostgreSQL")) {
		cast = "CAST(? AS TEXT)";
	} else if (DBMSConfigured("MySQL") || DBMSConfigured("MariaDB")) {
		cast = "CAST(? AS CHAR(20000))";
	} else if (DBMSConfigured("Spark")) {
		cast = "CAST(? AS STRING)";
	} else if (DBMSConfigured("ClickHouse")) {
		cast = "CAST(? AS Nullable(VARCHAR)) AS col1";
	} else if (DBMSConfigured("Oracle")) {
		cast = "CAST(? AS VARCHAR2(4000)) FROM dual";
	}
	ScannerConn sc;
	duckdb_prepared_statement ps_ptr = nullptr;
	duckdb_state st_prepare = duckdb_prepare(sc.conn,
	                                         std::string(R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT )" + cast + R"(
  ', 
  params=row(?::VARCHAR))
)")
	                                             .c_str(),
	                                         &ps_ptr);
	REQUIRE(PreparedSuccess(ps_ptr, st_prepare));
	auto ps = PreparedStatementPtr(ps_ptr, PreparedStatementDeleter);

	std::vector<std::string> vec;
	for (size_t i = (1 << 10) - 4; i < (1 << 10) + 4; i++) {
		std::string str = GenStr(i);
		vec.emplace_back(std::move(str));
	}
	for (size_t i = (1 << 11) - 4; i < (1 << 11) + 4; i++) {
		std::string str = GenStr(i);
		vec.emplace_back(std::move(str));
	}
	if (!DBMSConfigured("Oracle")) {
		for (size_t i = (1 << 12) - 4; i < (1 << 12) + 4; i++) {
			std::string str = GenStr(i);
			vec.emplace_back(std::move(str));
		}
		for (size_t i = (1 << 13) - 4; i < (1 << 13) + 4; i++) {
			std::string str = GenStr(i);
			vec.emplace_back(std::move(str));
		}
		for (size_t i = (1 << 14) - 4; i < (1 << 14) + 4; i++) {
			std::string str = GenStr(i);
			vec.emplace_back(std::move(str));
		}
		REQUIRE(vec.size() == 40);
	}

	for (std::string &str : vec) {
		auto param_val = ValuePtr(duckdb_create_varchar_length(str.c_str(), str.length()), ValueDeleter);
		duckdb_state st_bind = duckdb_bind_value(ps.get(), 1, param_val.get());
		REQUIRE(PreparedSuccess(ps_ptr, st_bind));

		Result res;
		duckdb_state st_exec = duckdb_execute_prepared(ps.get(), res.Get());
		REQUIRE(QuerySuccess(res.Get(), st_exec));
		REQUIRE(res.NextChunk());
		REQUIRE(res.Value<std::string>(0, 0) == str);
	}
}
