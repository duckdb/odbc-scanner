#include "mappings.hpp"

namespace odbcscanner {

std::unordered_map<duckdb_type, std::string> Mappings::Oracle(const DbmsQuirks &) {
	return {{DUCKDB_TYPE_BOOLEAN, "NUMBER(1,0)"},
	        {DUCKDB_TYPE_TINYINT, "NUMBER(3,0)"},
	        {DUCKDB_TYPE_UTINYINT, "NUMBER(3,0)"},
	        {DUCKDB_TYPE_SMALLINT, "NUMBER(5,0)"},
	        {DUCKDB_TYPE_SMALLINT, "NUMBER(5,0)"},
	        {DUCKDB_TYPE_INTEGER, "NUMBER(10,0)"},
	        {DUCKDB_TYPE_UINTEGER, "NUMBER(10,0)"},
	        {DUCKDB_TYPE_BIGINT, "NUMBER(19,0)"},
	        {DUCKDB_TYPE_UBIGINT, "NUMBER(19,0)"},
	        {DUCKDB_TYPE_HUGEINT, "NUMBER(39,0)"},
	        {DUCKDB_TYPE_UHUGEINT, "NUMBER(39,0)"},
	        {DUCKDB_TYPE_FLOAT, "BINARY_FLOAT"},
	        {DUCKDB_TYPE_DOUBLE, "BINARY_DOUBLE"},
	        {DUCKDB_TYPE_TIMESTAMP, "TIMESTAMP(6)"},
	        {DUCKDB_TYPE_DATE, "DATE"},
	        // { DUCKDB_TYPE_TIME, "TODO"},
	        // { DUCKDB_TYPE_INTERVAL, "TODO"},
	        // { DUCKDB_TYPE_INTERVAL, "TODO"},
	        {DUCKDB_TYPE_VARCHAR, "NVARCHAR2(2000)"},
	        {DUCKDB_TYPE_BLOB, "BLOB"},
	        // TODO: type attributes
	        {DUCKDB_TYPE_DECIMAL, "NUMBER(38,19)"},
	        {DUCKDB_TYPE_TIMESTAMP_S, "TIMESTAMP(0)"},
	        {DUCKDB_TYPE_TIMESTAMP_MS, "TIMESTAMP(3)"},
	        {DUCKDB_TYPE_TIMESTAMP_NS, "TIMESTAMP(9)"},
	        // { DUCKDB_TYPE_UUID, "TODO"},
	        {DUCKDB_TYPE_TIMESTAMP, "TIMESTAMP(6)"},
	        // { DUCKDB_TYPE_TIME_TZ, "TODO"},
	        {DUCKDB_TYPE_TIME_TZ, "TIMESTAMP(6) WITH LOCAL TIME ZONE"}};
}

} // namespace odbcscanner