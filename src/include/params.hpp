#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <sql.h>
#include <sqlext.h>

#include "capi_pointers.hpp"
#include "duckdb_extension.h"
#include "widechar.hpp"

namespace odbcscanner {

class ScannerParam {
	duckdb_type type_id = DUCKDB_TYPE_INVALID;
	SQLLEN len_bytes = 0;

	union Value {
		bool null_val;
		int32_t int32;
		int64_t int64;
		WideString wstr;

		Value() : null_val(true) {
		}
		Value(int32_t int32_in) : int32(int32_in) {
		}
		Value(int64_t int64_in) : int64(int64_in) {
		}
		Value(WideString wstr_in) : wstr(std::move(wstr_in)) {
		}

		Value(Value &other) = delete;
		Value(Value &&other) = delete;

		Value &operator=(Value &other) = delete;
		Value &operator=(Value &&other) = delete;

		~Value() noexcept {};
	} val;

public:
	ScannerParam();
	explicit ScannerParam(int32_t value);
	explicit ScannerParam(int64_t value);
	explicit ScannerParam(const char *cstr, size_t len);

	ScannerParam(ScannerParam &other) = delete;
	ScannerParam(ScannerParam &&other);

	ScannerParam &operator=(ScannerParam &other) = delete;
	ScannerParam &operator=(ScannerParam &&other);

	~ScannerParam();

	std::string ToUtf8String(size_t max_len = std::numeric_limits<size_t>::max());

	duckdb_type TypeId();

	SQLLEN &LengthBytes();

	int32_t &Int32();
	int64_t &Int64();
	WideString &Utf16String();

private:
	void CheckType(duckdb_type expected);
};

struct Params {
	static std::vector<ScannerParam> Extract(duckdb_data_chunk chunk, idx_t col_idx);

	static std::vector<ScannerParam> Extract(duckdb_value struct_value);

	static std::vector<SQLSMALLINT> CollectTypes(const std::string &query, HSTMT hstmt);

	static void CheckTypes(const std::string &query, const std::vector<SQLSMALLINT> &expected,
	                       std::vector<ScannerParam> &actual);

	static void BindToOdbc(const std::string &query, HSTMT hstmt, std::vector<ScannerParam> &params);
};

} // namespace odbcscanner
