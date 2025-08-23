#pragma once

#include <string>

#include <sql.h>
#include <sqlext.h>

#include "duckdb_extension.h"
#include "params.hpp"

namespace odbcscanner {

struct OdbcType {
	SQLLEN desc_type;
	SQLLEN desc_concise_type;
	std::string desc_type_name;

	explicit OdbcType(SQLLEN desc_type_in, SQLLEN desc_concise_type_in, std::string desc_type_name_in)
	    : desc_type(desc_type_in), desc_concise_type(desc_concise_type_in),
	      desc_type_name(std::move(desc_type_name_in)) {
	}

	OdbcType(OdbcType &other) = delete;
	OdbcType(OdbcType &&other) = default;

	OdbcType &operator=(OdbcType &other) = delete;
	OdbcType &operator=(OdbcType &&other) = default;

	std::string ToString();

	bool Equals(OdbcType &other);
};

struct Types {

	// Type-dispatched functions

	static ScannerParam ExtractNotNullParamOfType(duckdb_type type_id, duckdb_vector vec, idx_t param_idx);

	static ScannerParam ExtractNotNullParamOfType(duckdb_type type_id, duckdb_value value, idx_t param_idx);

	static void BindOdbcParam(const std::string &query, HSTMT hstmt, ScannerParam &param, SQLSMALLINT param_idx);

	static void AddResultColumnOfType(const OdbcType &odbc_type, duckdb_bind_info info, const std::string &name);

	static void FetchAndSetResultOfType(const OdbcType &odbc_type, const std::string &query, HSTMT hstmt,
	                                    SQLSMALLINT col_idx, duckdb_vector vec, idx_t row_idx);

	// Other functions
	template <typename T>
	static std::pair<T, bool> ExtractFunctionArg(duckdb_data_chunk chunk, idx_t col_idx);

	static void SetNullValueToResult(duckdb_vector vec, idx_t row_idx);

	static SQLSMALLINT DuckParamTypeToOdbc(duckdb_type type_id, size_t param_idx);
};

class TypeSpecific {
	friend struct Types;

	template <typename T>
	static ScannerParam ExtractNotNullParam(duckdb_vector vec);

	template <typename T>
	static ScannerParam ExtractNotNullParam(duckdb_value value);

	template <typename T>
	static void BindOdbcParam(const std::string &query, HSTMT hstmt, ScannerParam &param, SQLSMALLINT param_idx);

	template <typename T>
	static void AddResultColumn(duckdb_bind_info info, const std::string &name);

	template <typename T>
	static void FetchAndSetResult(const std::string &query, HSTMT hstmt, SQLSMALLINT col_idx, duckdb_vector vec,
	                              idx_t row_idx);
};

} // namespace odbcscanner
