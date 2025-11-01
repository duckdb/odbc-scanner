#include "dbms_quirks.hpp"

namespace odbcscanner {

const std::string DbmsQuirks::MSSQL_DBMS_NAME = "Microsoft SQL Server";
const std::string DbmsQuirks::MARIADB_DBMS_NAME = "MariaDB";
const std::string DbmsQuirks::MYSQL_DBMS_NAME = "MySQL";
const std::string DbmsQuirks::SPARK_DBMS_NAME = "Spark SQL";
const std::string DbmsQuirks::CLICKHOUSE_DBMS_NAME = "ClickHouse";
const std::string DbmsQuirks::ORACLE_DBMS_NAME = "Oracle";
const std::string DbmsQuirks::DB2_DBMS_NAME_PREFIX = "DB2/";

DbmsQuirks::DbmsQuirks() {
}

DbmsQuirks::DbmsQuirks(OdbcConnection &conn, const DbmsQuirks &user_quirks) {

	// Quirks assigned based on DBMS name reported by the driver

	if (conn.dbms_name == MSSQL_DBMS_NAME) {
		this->var_len_max_size_bytes = 8000;
		this->decimal_columns_precision_through_ard = true;
		this->decimal_params_as_chars = true;
		this->float_width_bytes = 8;
		this->time_params_as_ss_time2 = true;
		this->timestamp_max_fraction_precision = 7;
		this->timestamptz_params_as_ss_timestampoffset = true;

	} else if (conn.dbms_name == MARIADB_DBMS_NAME || conn.dbms_name == MYSQL_DBMS_NAME) {
		this->decimal_params_as_chars = true;
		this->decimal_columns_as_chars = true;

	} else if (conn.dbms_name == SPARK_DBMS_NAME) {
		this->decimal_params_as_chars = true;
		this->decimal_columns_as_chars = true;

	} else if (conn.dbms_name == CLICKHOUSE_DBMS_NAME) {
		this->decimal_params_as_chars = true;
		this->decimal_columns_as_chars = true;
		this->reset_stmt_before_execute = true;
		this->var_len_data_single_part = true;

	} else if (conn.dbms_name == ORACLE_DBMS_NAME) {
		this->decimal_columns_precision_through_ard = true;

	} else if (conn.dbms_name.rfind(DB2_DBMS_NAME_PREFIX, 0) == 0) {
		this->decimal_params_as_chars = true;
		this->decimal_columns_as_chars = true;
	}

	// Quirks explicitly requested by user

	if (user_quirks.datetime2_columns_as_timestamp_ns) {
		this->datetime2_columns_as_timestamp_ns = true;
	}
	if (user_quirks.timestamp_columns_as_timestamptz) {
		this->timestamp_columns_as_timestamptz = true;
	}
}

} // namespace odbcscanner
