#include "mappings.hpp"

namespace odbcscanner {

std::unordered_map<LogicalTypeId, std::string> Mappings::MSSQL(const DbmsQuirks &) {
	return {{LogicalTypeId::BOOLEAN, "BIT"},

	        {LogicalTypeId::TINYINT, "SMALLINT"},
	        {LogicalTypeId::UTINYINT, "SMALLINT"},
	        {LogicalTypeId::SMALLINT, "SMALLINT"},
	        {LogicalTypeId::USMALLINT, "INTEGER"},
	        {LogicalTypeId::INTEGER, "INTEGER"},
	        {LogicalTypeId::UINTEGER, "BIGINT"},
	        {LogicalTypeId::BIGINT, "BIGINT"},

	        {LogicalTypeId::FLOAT, "REAL"},
	        {LogicalTypeId::DOUBLE, "FLOAT"},

	        {LogicalTypeId::DECIMAL, "DECIMAL({typmod1},{typmod2})"},

	        {LogicalTypeId::DATE, "DATE"},
	        {LogicalTypeId::TIMESTAMP, "DATETIME2(6)"},
	        {LogicalTypeId::TIMESTAMP_NS, "DATETIME2(7)"},

	        {LogicalTypeId::VARCHAR, "NVARCHAR(MAX)"},
	        {LogicalTypeId::BLOB, "VARBINARY(MAX)"}};
}

} // namespace odbcscanner
