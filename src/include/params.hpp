#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <sql.h>
#include <sqlext.h>

#include "capi_pointers.hpp"
#include "duckdb_extension.h"
#include "widechar.hpp"

namespace odbcscanner {

struct ScannerParam {
	duckdb_type type_id = DUCKDB_TYPE_INVALID;

	// todo: union
	int32_t int32 = 0;
	int64_t int64 = 0;
	std::string str;
	SqlWString wstr;

	SQLLEN len_bytes = 0;

	ScannerParam();
	ScannerParam(int32_t value);
	ScannerParam(int64_t value);
	ScannerParam(std::string value);

	ScannerParam(ScannerParam &other) = delete;
	ScannerParam(ScannerParam &&other) = default;

	ScannerParam &operator=(ScannerParam &other) = delete;
	ScannerParam &operator=(ScannerParam &&other) = default;
};

std::vector<ScannerParam> ExtractStructParamsFromChunk(duckdb_data_chunk chunk, idx_t col_idx);

std::vector<ScannerParam> ExtractStructParamsFromValue(duckdb_value struct_value);

void SetOdbcParam(const std::string &query, HSTMT hstmt, ScannerParam &param, SQLSMALLINT param_idx);

void ResetOdbcParams(HSTMT hstmt);

} // namespace odbcscanner
