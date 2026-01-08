#pragma once

#include <map>
#include <string>
#include <vector>

#include "capi_pointers.hpp"
#include "connection.hpp"
#include "duckdb_extension_api.hpp"

namespace odbcscanner {

struct DbmsQuirks {

	bool decimal_columns_as_chars = false;
	bool decimal_columns_precision_through_ard = false;
	bool decimal_columns_precision_through_ard_bind = false;
	bool decimal_params_as_chars = false;
	bool integral_params_as_decimals = false;
	bool reset_stmt_before_execute = false;
	bool time_params_as_ss_time2 = false;
	bool timestamp_columns_as_timestamp_ns = false;
	bool timestamp_columns_with_typename_date_as_date = false;
	uint8_t timestamp_max_fraction_precision = 9;
	bool timestamp_params_as_sf_timestamp_ntz = false;
	bool timestamptz_params_as_ss_timestampoffset = false;
	bool var_len_data_single_part = false;
	uint32_t var_len_params_long_threshold_bytes = 4000;

	explicit DbmsQuirks(OdbcConnection &conn, const std::map<std::string, ValuePtr> &user_quirks);

	static const std::vector<std::string> AllNames();
};

} // namespace odbcscanner
