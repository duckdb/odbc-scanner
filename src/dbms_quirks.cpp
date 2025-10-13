
#include "dbms_quirks.hpp"

namespace odbcscanner {

const std::string DbmsQuirks::MSSQL_DBMS_NAME = "Microsoft SQL Server";
const std::string DbmsQuirks::MARIADB_DBMS_NAME = "MariaDB";
const std::string DbmsQuirks::MYSQL_DBMS_NAME = "MySQL";

DbmsQuirks::DbmsQuirks() {
}

DbmsQuirks::DbmsQuirks(OdbcConnection &conn) {
	if (conn.dbms_name == MSSQL_DBMS_NAME) {
		this->varchar_max_size_bytes = 8000;
		this->decimal_columns_precision_through_ard = true;
		this->decimal_params_as_chars = true;
		this->float_width_bytes = 8;
	} else if (conn.dbms_name == MARIADB_DBMS_NAME || conn.dbms_name == MYSQL_DBMS_NAME) {
		this->decimal_params_as_chars = true;
		this->decimal_columns_as_chars = true;
	}
}

} // namespace odbcscanner
