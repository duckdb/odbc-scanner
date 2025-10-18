
#include "dbms_quirks.hpp"

namespace odbcscanner {

const std::string DbmsQuirks::MSSQL_DBMS_NAME = "Microsoft SQL Server";
const std::string DbmsQuirks::MARIADB_DBMS_NAME = "MariaDB";
const std::string DbmsQuirks::MYSQL_DBMS_NAME = "MySQL";
const std::string DbmsQuirks::SPARK_DBMS_NAME = "Spark SQL";
const std::string DbmsQuirks::CLICKHOUSE_DBMS_NAME = "ClickHouse";

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
	} else if (conn.dbms_name == SPARK_DBMS_NAME) {
		this->decimal_params_as_chars = true;
		this->decimal_columns_as_chars = true;
	} else if (conn.dbms_name == CLICKHOUSE_DBMS_NAME) {
		this->reset_stmt_before_execute = true;
		this->var_len_data_single_part = true;
	}
}

} // namespace odbcscanner
