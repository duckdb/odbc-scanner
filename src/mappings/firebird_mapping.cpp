#include "mappings.hpp"

namespace odbcscanner {

std::unordered_map<duckdb_type, std::string> Mappings::FIREBIRD(const DbmsQuirks &) {
	return {{DUCKDB_TYPE_BOOLEAN, "BOOLEAN"},

	        {DUCKDB_TYPE_TINYINT, "SMALLINT"},
	        {DUCKDB_TYPE_UTINYINT, "SMALLINT"},
	        {DUCKDB_TYPE_SMALLINT, "SMALLINT"},
	        {DUCKDB_TYPE_USMALLINT, "INTEGER"},
	        {DUCKDB_TYPE_INTEGER, "INTEGER"},
	        {DUCKDB_TYPE_UINTEGER, "BIGINT"},
	        {DUCKDB_TYPE_BIGINT, "BIGINT"},

	        {DUCKDB_TYPE_FLOAT, "REAL"},
	        {DUCKDB_TYPE_DOUBLE, "DOUBLE PRECISION"},

	        {DUCKDB_TYPE_DECIMAL, "DECIMAL({typmod1},{typmod2})"},

	        {DUCKDB_TYPE_DATE, "DATE"},
	        {DUCKDB_TYPE_TIMESTAMP, "TIMESTAMP"},

	        {DUCKDB_TYPE_VARCHAR, "BLOB SUB_TYPE TEXT"},
	        {DUCKDB_TYPE_BLOB, "BLOB"}};
}

} // namespace odbcscanner