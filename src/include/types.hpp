#pragma once

#include <string>

#include "dbms_quirks.hpp"
#include "duckdb_extension_api.hpp"
#include "odbc_api.hpp"
#include "params.hpp"
#include "query_context.hpp"

namespace odbcscanner {

struct ResultColumn;

struct OdbcType {
	SQLLEN desc_type;
	SQLLEN desc_concise_type;
	bool is_unsigned;
	std::string desc_type_name;
	uint8_t decimal_precision;
	uint8_t decimal_scale;

	explicit OdbcType(SQLLEN desc_type_in, SQLLEN desc_concise_type_in, bool is_unsigned_in,
	                  std::string desc_type_name_in, uint8_t decimal_precision_in, uint8_t decimal_scale_in)
	    : desc_type(desc_type_in), desc_concise_type(desc_concise_type_in), is_unsigned(is_unsigned_in),
	      desc_type_name(std::move(desc_type_name_in)), decimal_precision(decimal_precision_in),
	      decimal_scale(decimal_scale_in) {
	}

	OdbcType(OdbcType &other) = delete;
	OdbcType(OdbcType &&other) = default;

	OdbcType &operator=(OdbcType &other) = delete;
	OdbcType &operator=(OdbcType &&other) = default;

	std::string ToString();

	bool Equals(OdbcType &other);
};

struct Types {

	static const SQLSMALLINT SQL_SS_TIME2 = -154;
	static const SQLSMALLINT SQL_SS_TIMESTAMPOFFSET = -155;

	static const SQLSMALLINT SQL_SF_TIMESTAMP_LTZ = 2000;
	static const SQLSMALLINT SQL_SF_TIMESTAMP_TZ = 2001;
	static const SQLSMALLINT SQL_SF_TIMESTAMP_NTZ = 2002;

	static const SQLSMALLINT SQL_DB2_BLOB = -98;
	static const SQLSMALLINT SQL_DB2_CLOB = -99;
	static const SQLSMALLINT SQL_DB2_DBCLOB = -350;

	static const std::string MSSQL_DATETIME2_TYPE_NAME;
	static const std::string SQL_DATE_TYPE_NAME;
	static const std::string SQL_BLOB_TYPE_NAME;
	static const std::string SQL_CLOB_TYPE_NAME;
	static const std::string SQL_DB2_DBCLOB_TYPE_NAME;

	// Type-dispatched functions

	static ScannerValue ExtractNotNullParam(DbmsQuirks &quirks, duckdb_type type_id, duckdb_vector vec, idx_t row_idx,
	                                        idx_t param_idx);

	static ScannerValue ExtractNotNullParam(DbmsQuirks &quirks, duckdb_value value, idx_t param_idx);

	static void BindOdbcParam(QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx);

	static void BindColumn(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx);

	static void FetchAndSetResult(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx, duckdb_vector vec,
	                              idx_t row_idx);

	static void CoalesceParameterType(QueryContext &ctx, ScannerValue &param);

	static void CoalesceColumnType(QueryContext &ctx, ResultColumn &column);

	static duckdb_type ResolveColumnType(QueryContext &ctx, ResultColumn &column);

	// Other functions

	template <typename T>
	static std::pair<T, bool> ExtractFunctionArg(duckdb_data_chunk chunk, idx_t col_idx);

	static void SetNullValueToResult(duckdb_vector vec, idx_t row_idx);
};

class TypeSpecific {
	friend struct Types;

	template <typename T>
	static ScannerValue ExtractNotNullParam(DbmsQuirks &quirks, duckdb_vector vec, idx_t row_idx);

	template <typename T>
	static ScannerValue ExtractNotNullParam(DbmsQuirks &quirks, duckdb_value value);

	template <typename T>
	static void BindOdbcParam(QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx);

	template <typename T>
	static void BindColumn(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx);

	template <typename T>
	static void FetchAndSetResult(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx, duckdb_vector vec,
	                              idx_t row_idx);

	template <typename T>
	static duckdb_type ResolveColumnType(QueryContext &ctx, ResultColumn &column);
};

} // namespace odbcscanner
