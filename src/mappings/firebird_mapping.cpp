#include "mappings.hpp"

namespace odbcscanner {

std::unordered_map<LogicalTypeId, std::string> Mappings::FIREBIRD(const DbmsQuirks &) {
	return {{LogicalTypeId::BOOLEAN, "BOOLEAN"},

	        {LogicalTypeId::TINYINT, "SMALLINT"},
	        {LogicalTypeId::UTINYINT, "SMALLINT"},
	        {LogicalTypeId::SMALLINT, "SMALLINT"},
	        {LogicalTypeId::USMALLINT, "INTEGER"},
	        {LogicalTypeId::INTEGER, "INTEGER"},
	        {LogicalTypeId::UINTEGER, "BIGINT"},
	        {LogicalTypeId::BIGINT, "BIGINT"},
	        {LogicalTypeId::UBIGINT, "INT128"},

	        {LogicalTypeId::FLOAT, "REAL"},
	        {LogicalTypeId::DOUBLE, "DOUBLE PRECISION"},

	        {LogicalTypeId::DECIMAL, "DECIMAL({typmod1},{typmod2})"},

	        {LogicalTypeId::DATE, "DATE"},
	        {LogicalTypeId::TIMESTAMP, "TIMESTAMP"},

	        {LogicalTypeId::VARCHAR, "BLOB SUB_TYPE TEXT"},
	        {LogicalTypeId::BLOB, "BLOB"}};
}

} // namespace odbcscanner