#pragma once

#include <string>
#include <unordered_map>

#include "connection.hpp"
#include "dbms_quirks.hpp"
#include "duckdb_extension_api.hpp"

namespace odbcscanner {

struct Mappings {

	static std::unordered_map<duckdb_type, std::string> Resolve(DbmsDriver driver, const DbmsQuirks &quirks);

	static std::unordered_map<duckdb_type, std::string> Generic(const DbmsQuirks &quirks);

	static std::unordered_map<duckdb_type, std::string> Oracle(const DbmsQuirks &quirks);

	static std::unordered_map<duckdb_type, std::string> MSSQL(const DbmsQuirks &quirks);

	static std::unordered_map<duckdb_type, std::string> DB2(const DbmsQuirks &quirks);
};

} // namespace odbcscanner
