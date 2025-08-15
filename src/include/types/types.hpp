#pragma once

#include "duckdb_extension.h"

#include <string>

#include <sql.h>
#include <sqlext.h>

namespace odbcscanner {

struct OdbcType {
  SQLLEN desc_type;
  SQLLEN desc_concise_type;
  std::string desc_type_name;

	OdbcType(SQLLEN desc_type_in, SQLLEN desc_concise_type_in, std::string desc_type_name_in) :
  desc_type(desc_type_in), desc_concise_type(desc_concise_type_in), desc_type_name(std::move(desc_type_name_in)) {}

	OdbcType(OdbcType &other) = delete;
	OdbcType(OdbcType &&other) = default;

	OdbcType &operator=(OdbcType &other) = delete;
	OdbcType &operator=(OdbcType &&other) = default;
};

OdbcType GetResultColumnTypeAttributes(const std::string &query, SQLSMALLINT cols_count, HSTMT hstmt, SQLUSMALLINT col_idx);

void AddResultColumn(duckdb_bind_info info, const std::string &name, const OdbcType &odbc_ctype);

} // namespace odbcscanner
