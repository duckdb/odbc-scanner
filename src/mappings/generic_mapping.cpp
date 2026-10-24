#include "mappings.hpp"

namespace odbcscanner {

std::unordered_map<LogicalTypeId, std::string> Mappings::Generic(const DbmsQuirks &) {
	return {{LogicalTypeId::BOOLEAN, "INTEGER"},   {LogicalTypeId::TINYINT, "INTEGER"},
	        {LogicalTypeId::UTINYINT, "INTEGER"},  {LogicalTypeId::SMALLINT, "INTEGER"},
	        {LogicalTypeId::USMALLINT, "INTEGER"}, {LogicalTypeId::INTEGER, "INTEGER"},
	        {LogicalTypeId::UINTEGER, "INTEGER"}};
}

} // namespace odbcscanner
