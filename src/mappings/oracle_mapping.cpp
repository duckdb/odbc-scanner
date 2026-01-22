#include "mappings.hpp"

namespace odbcscanner {

std::unordered_map<duckdb_type, std::string> Mappings::Oracle(const DbmsQuirks &) {
	return {{DUCKDB_TYPE_BOOLEAN, "NUMBER(1,0)"},

	        {DUCKDB_TYPE_TINYINT, "NUMBER(3,0)"},
	        {DUCKDB_TYPE_UTINYINT, "NUMBER(3,0)"},
	        {DUCKDB_TYPE_SMALLINT, "NUMBER(5,0)"},
	        {DUCKDB_TYPE_USMALLINT, "NUMBER(5,0)"},
	        {DUCKDB_TYPE_INTEGER, "NUMBER(10,0)"},
	        {DUCKDB_TYPE_UINTEGER, "NUMBER(10,0)"},
	        {DUCKDB_TYPE_BIGINT, "NUMBER(19,0)"},
	        {DUCKDB_TYPE_UBIGINT, "NUMBER(19,0)"},

	        {DUCKDB_TYPE_FLOAT, "BINARY_FLOAT"},
	        {DUCKDB_TYPE_DOUBLE, "BINARY_DOUBLE"},

	        {DUCKDB_TYPE_DECIMAL, "NUMBER({typmod1},{typmod2})"},

	        {DUCKDB_TYPE_DATE, "DATE"},
	        {DUCKDB_TYPE_TIMESTAMP, "TIMESTAMP(6)"},
	        {DUCKDB_TYPE_TIMESTAMP_NS, "TIMESTAMP(9)"},

	        {DUCKDB_TYPE_VARCHAR, "NVARCHAR2(2000)"},
	        {DUCKDB_TYPE_BLOB, "BLOB"}};
}

} // namespace odbcscanner
