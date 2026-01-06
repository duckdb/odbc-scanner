#include "mappings.hpp"

namespace odbcscanner {

std::unordered_map<duckdb_type, std::string> Mappings::Generic(const DbmsQuirks &) {
	return {
	    {DUCKDB_TYPE_BOOLEAN, "INTEGER"}, {DUCKDB_TYPE_TINYINT, "INTEGER"}
	    // todo
	};
}

} // namespace odbcscanner
