#include "test_common.hpp"

#include <vector>

static std::string GenStr(size_t len) {
	std::string res;
	for (size_t i = 0; i < len; i++) {
		std::string ch = std::to_string(i % 10);
		res.append(ch);
	}
	REQUIRE(res.length() == len);
	return res;
}

TEST_CASE("Long string query", "[capi_long_strings]") {
	ScannerConn sc;
	duckdb_prepared_statement ps_ptr = nullptr;
	duckdb_state st_prepare = duckdb_prepare(sc.conn, R"(
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT ?::VARCHAR
  ', 
  params=row(?::VARCHAR))
)",
	                                         &ps_ptr);
	REQUIRE(st_prepare == DuckDBSuccess);
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

	for (std::string &str : vec) {
		auto param_val = ValuePtr(duckdb_create_varchar_length(str.c_str(), str.length()), ValueDeleter);
		duckdb_state st_bind = duckdb_bind_value(ps.get(), 1, param_val.get());
		REQUIRE(st_bind == DuckDBSuccess);

		Result res;
		duckdb_state st_exec = duckdb_execute_prepared(ps.get(), res.Get());
		REQUIRE(st_exec == DuckDBSuccess);
		REQUIRE(res.NextChunk());
		REQUIRE(res.Value<std::string>(0, 0) == str);
	}
}
