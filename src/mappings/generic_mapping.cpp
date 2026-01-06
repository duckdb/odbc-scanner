#include "mappings.hpp"

namespace odbcscanner {

std::unordered_map<duckdb_type, std::string> Mappings::Generic(const DbmsQuirks &) {
	return {{DUCKDB_TYPE_BOOLEAN, "INTEGER"},  {DUCKDB_TYPE_TINYINT, "INTEGER"},   {DUCKDB_TYPE_UTINYINT, "INTEGER"},
	        {DUCKDB_TYPE_SMALLINT, "INTEGER"}, {DUCKDB_TYPE_USMALLINT, "INTEGER"}, {DUCKDB_TYPE_INTEGER, "INTEGER"},
	        {DUCKDB_TYPE_UINTEGER, "INTEGER"}};
}

} // namespace odbcscanner
