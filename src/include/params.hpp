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

	union IntenalValue {
		bool null_val;
		int8_t int8;
		uint8_t uint8;
		int16_t int16;
		uint16_t uint16;
		int32_t int32;
		uint32_t uint32;
		int64_t int64;
		uint64_t uint64;
		WideString wstr;

		IntenalValue() : null_val(true) {
		}
		IntenalValue(int8_t value) : int8(value) {
		}
		IntenalValue(uint8_t value) : uint8(value) {
		}
		IntenalValue(int16_t value) : int16(value) {
		}
		IntenalValue(uint16_t value) : uint16(value) {
		}
		IntenalValue(int32_t value) : int32(value) {
		}
		IntenalValue(uint32_t value) : uint32(value) {
		}
		IntenalValue(int64_t value) : int64(value) {
		}
		IntenalValue(uint64_t value) : uint64(value) {
		}
		IntenalValue(WideString wstr_in) : wstr(std::move(wstr_in)) {
		}

		IntenalValue(IntenalValue &other) = delete;
		IntenalValue(IntenalValue &&other) = delete;

		IntenalValue &operator=(IntenalValue &other) = delete;
		IntenalValue &operator=(IntenalValue &&other) = delete;

		~IntenalValue() noexcept {};
	} val;

public:
	ScannerParam();
	explicit ScannerParam(int8_t value);
	explicit ScannerParam(uint8_t value);
	explicit ScannerParam(int16_t value);
	explicit ScannerParam(uint16_t value);
	explicit ScannerParam(int32_t value);
	explicit ScannerParam(uint32_t value);
	explicit ScannerParam(int64_t value);
	explicit ScannerParam(uint64_t value);
	explicit ScannerParam(const char *cstr, size_t len);
	explicit ScannerParam(const char *cstr);

	ScannerParam(ScannerParam &other) = delete;
	ScannerParam(ScannerParam &&other);

	ScannerParam &operator=(ScannerParam &other) = delete;
	ScannerParam &operator=(ScannerParam &&other);

	~ScannerParam();

	std::string ToUtf8String(size_t max_len = std::numeric_limits<size_t>::max());

	duckdb_type TypeId();

	SQLLEN &LengthBytes();

	template <typename T>
	T &Value();

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
