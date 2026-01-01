#include "dbms_quirks.hpp"

#include "scanner_exception.hpp"

namespace odbcscanner {

static const std::string MSSQL_DBMS_NAME = "Microsoft SQL Server";
static const std::string MARIADB_DBMS_NAME = "MariaDB";
static const std::string MYSQL_DBMS_NAME = "MySQL";
static const std::string SPARK_DBMS_NAME = "Spark SQL";
static const std::string CLICKHOUSE_DBMS_NAME = "ClickHouse";
static const std::string ORACLE_DBMS_NAME = "Oracle";
static const std::string DB2_DBMS_NAME_PREFIX = "DB2/";
static const std::string SNOWFLAKE_DBMS_NAME = "Snowflake";
static const std::string FLIGTHSQL_DRIVER_NAME = "Arrow Flight ODBC Driver";

DbmsQuirks::DbmsQuirks(OdbcConnection &conn, const std::map<std::string, ValuePtr> &user_quirks) {

	// Quirks assigned based on DBMS name reported by the driver

	if (conn.dbms_name == MSSQL_DBMS_NAME) {
		this->var_len_params_long_threshold_bytes = 8000;
		this->decimal_columns_precision_through_ard = true;
		this->decimal_params_as_chars = true;
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
		this->var_len_params_long_threshold_bytes = 4000;
		this->decimal_columns_precision_through_ard = true;
		this->decimal_columns_bind = true;
		this->timestamp_columns_with_typename_date_as_date = true;

	} else if (conn.dbms_name.rfind(DB2_DBMS_NAME_PREFIX, 0) == 0) {
		this->decimal_params_as_chars = true;
		this->decimal_columns_as_chars = true;

	} else if (conn.dbms_name == SNOWFLAKE_DBMS_NAME) {
		this->decimal_columns_precision_through_ard = true;
		this->decimal_params_as_chars = true;
		this->timestamp_params_as_sf_timestamp_ntz = true;

	} else if (conn.driver_name == FLIGTHSQL_DRIVER_NAME) {
		this->decimal_columns_as_chars = true;
	}

	// Quirks explicitly requested by user

	for (auto &en : user_quirks) {
		const ValuePtr &val = en.second;
		if (!val || duckdb_is_null_value(val.get())) {
			continue;
		}
		if (en.first == "decimal_columns_as_chars") {
			this->timestamp_columns_as_timestamp_ns = duckdb_get_bool(val.get());
		} else if (en.first == "decimal_columns_precision_through_ard") {
			this->decimal_columns_precision_through_ard = duckdb_get_bool(val.get());
		} else if (en.first == "decimal_params_as_chars") {
			this->decimal_params_as_chars = duckdb_get_bool(val.get());
		} else if (en.first == "reset_stmt_before_execute") {
			this->reset_stmt_before_execute = duckdb_get_bool(val.get());
		} else if (en.first == "time_params_as_ss_time2") {
			this->time_params_as_ss_time2 = duckdb_get_bool(val.get());
		} else if (en.first == "timestamp_columns_as_timestamp_ns") {
			this->timestamp_columns_as_timestamp_ns = duckdb_get_bool(val.get());
		} else if (en.first == "timestamp_columns_with_typename_date_as_date") {
			this->timestamp_columns_with_typename_date_as_date = duckdb_get_bool(val.get());
		} else if (en.first == "timestamp_max_fraction_precision") {
			uint8_t num = duckdb_get_uint8(val.get());
			if (num > 9) {
				throw ScannerException("Invalid value for user option 'timestamp_max_fraction_precision': " +
				                       std::to_string(num) + ", max value: 9");
			}
			this->timestamp_max_fraction_precision = num;
		} else if (en.first == "timestamp_params_as_sf_timestamp_ntz") {
			this->timestamp_params_as_sf_timestamp_ntz = duckdb_get_bool(val.get());
		} else if (en.first == "timestamptz_params_as_ss_timestampoffset") {
			this->timestamptz_params_as_ss_timestampoffset = duckdb_get_bool(val.get());
		} else if (en.first == "var_len_data_single_part") {
			this->var_len_data_single_part = duckdb_get_bool(val.get());
		} else if (en.first == "var_len_params_long_threshold_bytes") {
			uint32_t num = duckdb_get_uint32(val.get());
			this->var_len_params_long_threshold_bytes = num;
		} else if (en.first == "timestamp_columns_as_timestamp_ns") {
			this->timestamp_columns_as_timestamp_ns = duckdb_get_bool(val.get());
		} else {
			throw ScannerException("Unsupported user option: '" + en.first + "'");
		}
	}
}

const std::vector<std::string> DbmsQuirks::AllNames() {
	std::vector<std::string> res;
	res.emplace_back("decimal_columns_as_chars");
	res.emplace_back("decimal_columns_precision_through_ard");
	res.emplace_back("decimal_params_as_chars");
	res.emplace_back("reset_stmt_before_execute");
	res.emplace_back("time_params_as_ss_time2");
	res.emplace_back("timestamp_columns_as_timestamp_ns");
	res.emplace_back("timestamp_columns_with_typename_date_as_date");
	res.emplace_back("timestamp_max_fraction_precision");
	res.emplace_back("timestamp_params_as_sf_timestamp_ntz");
	res.emplace_back("timestamptz_params_as_ss_timestampoffset");
	res.emplace_back("var_len_data_single_part");
	res.emplace_back("var_len_params_long_threshold_bytes");
	return res;
}

} // namespace odbcscanner
