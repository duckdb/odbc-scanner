#include "mappings.hpp"

namespace odbcscanner {

std::unordered_map<duckdb_type, std::string> Mappings::MSSQL(const DbmsQuirks &) {
	return {{DUCKDB_TYPE_BOOLEAN, "BIT"},

	        {DUCKDB_TYPE_TINYINT, "SMALLINT"},
	        {DUCKDB_TYPE_UTINYINT, "SMALLINT"},
	        {DUCKDB_TYPE_SMALLINT, "SMALLINT"},
	        {DUCKDB_TYPE_USMALLINT, "INTEGER"},
	        {DUCKDB_TYPE_INTEGER, "INTEGER"},
	        {DUCKDB_TYPE_UINTEGER, "BIGINT"},
	        {DUCKDB_TYPE_BIGINT, "BIGINT"},

	        {DUCKDB_TYPE_FLOAT, "REAL"},
	        {DUCKDB_TYPE_DOUBLE, "FLOAT"},

	        {DUCKDB_TYPE_DECIMAL, "DECIMAL({typmod1},{typmod2})"},

	        {DUCKDB_TYPE_DATE, "DATE"},
	        {DUCKDB_TYPE_TIMESTAMP, "DATETIME2(6)"},
	        {DUCKDB_TYPE_TIMESTAMP_NS, "DATETIME2(7)"},

	        {DUCKDB_TYPE_VARCHAR, "NVARCHAR(MAX)"},
	        {DUCKDB_TYPE_BLOB, "VARBINARY(MAX)"}};
}

} // namespace odbcscanner
