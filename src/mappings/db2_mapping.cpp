#include "mappings.hpp"

namespace odbcscanner {

std::unordered_map<LogicalTypeId, std::string> Mappings::DB2(const DbmsQuirks &) {
	return {{LogicalTypeId::BOOLEAN, "BOOLEAN"},

	        {LogicalTypeId::TINYINT, "SMALLINT"},
	        {LogicalTypeId::UTINYINT, "SMALLINT"},
	        {LogicalTypeId::SMALLINT, "SMALLINT"},
	        {LogicalTypeId::USMALLINT, "INTEGER"},
	        {LogicalTypeId::INTEGER, "INTEGER"},
	        {LogicalTypeId::UINTEGER, "BIGINT"},
	        {LogicalTypeId::BIGINT, "BIGINT"},

	        {LogicalTypeId::FLOAT, "REAL"},
	        {LogicalTypeId::DOUBLE, "DOUBLE"},

	        {LogicalTypeId::DECIMAL, "DECIMAL({typmod1},{typmod2})"},

	        {LogicalTypeId::DATE, "DATE"},
	        {LogicalTypeId::TIMESTAMP, "TIMESTAMP(6)"},
	        {LogicalTypeId::TIMESTAMP_NS, "TIMESTAMP(9)"},

	        {LogicalTypeId::VARCHAR, "VARCHAR(32672)"},
	        {LogicalTypeId::BLOB, "VARBINARY(32672)"}};
}

} // namespace odbcscanner
