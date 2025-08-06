#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

#include "catch.hpp"
#include "duckdb.h"

using LogicalTypePtr = std::unique_ptr<_duckdb_logical_type, void (*)(duckdb_logical_type)>;

inline void LogicalTypeDeleter(duckdb_logical_type lt) {
	duckdb_destroy_logical_type(&lt);
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

  int32_t Int32(idx_t col_idx, idx_t row_idx);

  int64_t Int64(idx_t col_idx, idx_t row_idx);

  std::string String(idx_t col_idx, idx_t row_idx);
};
