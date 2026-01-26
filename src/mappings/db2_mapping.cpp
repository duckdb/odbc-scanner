#include "mappings.hpp"

namespace odbcscanner {

std::unordered_map<duckdb_type, std::string> Mappings::DB2(const DbmsQuirks &) {
	return {{DUCKDB_TYPE_BOOLEAN, "BOOLEAN"},

	        {DUCKDB_TYPE_TINYINT, "SMALLINT"},
	        {DUCKDB_TYPE_UTINYINT, "SMALLINT"},
	        {DUCKDB_TYPE_SMALLINT, "SMALLINT"},
	        {DUCKDB_TYPE_USMALLINT, "INTEGER"},
	        {DUCKDB_TYPE_INTEGER, "INTEGER"},
	        {DUCKDB_TYPE_UINTEGER, "BIGINT"},
	        {DUCKDB_TYPE_BIGINT, "BIGINT"},

	        {DUCKDB_TYPE_FLOAT, "REAL"},
	        {DUCKDB_TYPE_DOUBLE, "DOUBLE"},

	        {DUCKDB_TYPE_DECIMAL, "DECIMAL({typmod1},{typmod2})"},

	        {DUCKDB_TYPE_DATE, "DATE"},
	        {DUCKDB_TYPE_TIMESTAMP, "TIMESTAMP(6)"},
	        {DUCKDB_TYPE_TIMESTAMP_NS, "TIMESTAMP(9)"},

	        {DUCKDB_TYPE_VARCHAR, "VARCHAR(32672)"},
	        {DUCKDB_TYPE_BLOB, "VARBINARY(32672)"}};
}

} // namespace odbcscanner
