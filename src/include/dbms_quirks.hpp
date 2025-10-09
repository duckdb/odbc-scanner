#pragma once

#include <string>

#include "connection.hpp"

namespace odbcscanner {

struct DbmsQuirks {
	static const std::string MSSQL_DBMS_NAME;

	size_t varchar_max_size_bytes = 0;
	bool decimal_precision_through_ard = false;
	bool decimal_params_as_chars = false;

	DbmsQuirks();

	explicit DbmsQuirks(OdbcConnection &conn);
};

} // namespace odbcscanner
