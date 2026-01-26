
#include "mappings.hpp"

namespace odbcscanner {

std::unordered_map<duckdb_type, std::string> Mappings::Resolve(DbmsDriver driver, const DbmsQuirks &quirks) {
	(void)quirks;
	switch (driver) {
	case DbmsDriver::ORACLE:
		return Oracle(quirks);
	case DbmsDriver::MSSQL:
		return MSSQL(quirks);
	case DbmsDriver::DB2:
		return DB2(quirks);

	case DbmsDriver::MARIADB:
	case DbmsDriver::MYSQL:

	case DbmsDriver::SNOWFLAKE:
	case DbmsDriver::SPARK:
	case DbmsDriver::CLICKHOUSE:
	case DbmsDriver::FLIGTHSQL:
	default:
		return Generic(quirks);
	}
}

} // namespace odbcscanner
