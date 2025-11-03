#pragma once

#include <string>

#include "connection.hpp"

namespace odbcscanner {

struct DbmsQuirks {
	static const std::string MSSQL_DBMS_NAME;
	static const std::string MARIADB_DBMS_NAME;
	static const std::string MYSQL_DBMS_NAME;
	static const std::string SPARK_DBMS_NAME;
	static const std::string CLICKHOUSE_DBMS_NAME;
	static const std::string ORACLE_DBMS_NAME;
	static const std::string DB2_DBMS_NAME_PREFIX;

	size_t var_len_params_long_threshold_bytes = 4000;
	bool decimal_columns_precision_through_ard = false;
	bool decimal_params_as_chars = false;
	bool decimal_columns_as_chars = false;
	bool reset_stmt_before_execute = false;
	bool var_len_data_single_part = false;
	bool time_params_as_ss_time2 = false;
	uint8_t timestamp_max_fraction_precision = 9;
	bool timestamp_columns_as_timestamp_ns = false;
	bool timestamptz_params_as_ss_timestampoffset = false;
	bool timestamp_columns_with_typename_date_as_date = false;

	DbmsQuirks();

	explicit DbmsQuirks(OdbcConnection &conn, const DbmsQuirks &user_quirks);
};

} // namespace odbcscanner
