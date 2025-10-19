#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "catch.hpp"
#include "duckdb.h"

using LogicalTypePtr = std::unique_ptr<_duckdb_logical_type, void (*)(duckdb_logical_type)>;

inline void LogicalTypeDeleter(duckdb_logical_type lt) {
	duckdb_destroy_logical_type(&lt);
}

using PreparedStatementPtr = std::unique_ptr<_duckdb_prepared_statement, void (*)(duckdb_prepared_statement)>;

inline void PreparedStatementDeleter(duckdb_prepared_statement ps) {
	duckdb_destroy_prepare(&ps);
}

using ValuePtr = std::unique_ptr<_duckdb_value, void (*)(duckdb_value)>;

inline void ValueDeleter(duckdb_value val) {
	duckdb_destroy_value(&val);
}

struct ScannerConn {
	duckdb_database db = nullptr;
	duckdb_connection conn = nullptr;

	ScannerConn();

	~ScannerConn() noexcept;
};

struct Result {
	duckdb_result res;
	duckdb_data_chunk chunk = nullptr;
	idx_t cur_row_idx = 0;

	~Result() noexcept;

	duckdb_result *Get();

	bool NextChunk();

	bool IsNull(idx_t col_idx, idx_t row_idx);

	template<typename T>
	T Value(idx_t col_idx, idx_t row_idx);

	template<typename T>
	T DecimalValue(idx_t col_idx, idx_t row_idx);
};

bool DBMSConfigured(const std::string dbms_name);

bool QuerySuccess(duckdb_result *res, duckdb_state st);

bool PreparedSuccess(duckdb_prepared_statement ps, duckdb_state st);

std::string CastAsBigintSQL(const std::string& value);

std::string CastAsDateSQL(const std::string& value);

std::string CastAsDecimalSQL(const std::string& value, uint8_t precision, uint8_t scale);
