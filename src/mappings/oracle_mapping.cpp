#include "mappings.hpp"

namespace odbcscanner {

std::unordered_map<LogicalTypeId, std::string> Mappings::Oracle(const DbmsQuirks &) {
	return {{LogicalTypeId::BOOLEAN, "NUMBER(1,0)"},

	        {LogicalTypeId::TINYINT, "NUMBER(3,0)"},
	        {LogicalTypeId::UTINYINT, "NUMBER(3,0)"},
	        {LogicalTypeId::SMALLINT, "NUMBER(5,0)"},
	        {LogicalTypeId::USMALLINT, "NUMBER(5,0)"},
	        {LogicalTypeId::INTEGER, "NUMBER(10,0)"},
	        {LogicalTypeId::UINTEGER, "NUMBER(10,0)"},
	        {LogicalTypeId::BIGINT, "NUMBER(19,0)"},
	        {LogicalTypeId::UBIGINT, "NUMBER(19,0)"},

	        {LogicalTypeId::FLOAT, "BINARY_FLOAT"},
	        {LogicalTypeId::DOUBLE, "BINARY_DOUBLE"},

	        {LogicalTypeId::DECIMAL, "NUMBER({typmod1},{typmod2})"},

	        {LogicalTypeId::DATE, "DATE"},
	        {LogicalTypeId::TIMESTAMP, "TIMESTAMP(6)"},
	        {LogicalTypeId::TIMESTAMP_NS, "TIMESTAMP(9)"},

	        {LogicalTypeId::VARCHAR, "NVARCHAR2(2000)"},
	        {LogicalTypeId::BLOB, "BLOB"}};
}

} // namespace odbcscanner
