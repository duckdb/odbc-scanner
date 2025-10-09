
#include "dbms_quirks.hpp"

namespace odbcscanner {

const std::string DbmsQuirks::MSSQL_DBMS_NAME = "Microsoft SQL Server";

DbmsQuirks::DbmsQuirks() {
}

DbmsQuirks::DbmsQuirks(OdbcConnection &conn) {
	if (conn.dbms_name == MSSQL_DBMS_NAME) {
		this->varchar_max_size_bytes = 8000;
		this->decimal_precision_through_ard = true;
		this->decimal_params_as_chars = true;
	}
}

} // namespace odbcscanner
