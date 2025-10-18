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

	size_t varchar_max_size_bytes = 0;
	bool decimal_columns_precision_through_ard = false;
	bool decimal_params_as_chars = false;
	bool decimal_columns_as_chars = false;
	uint8_t float_width_bytes = 4;
	bool reset_stmt_before_execute = false;
	bool var_len_data_single_part = false;

	DbmsQuirks();

	explicit DbmsQuirks(OdbcConnection &conn);
};

} // namespace odbcscanner
