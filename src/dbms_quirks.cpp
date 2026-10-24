#include "dbms_quirks.hpp"

#include "scanner_exception.hpp"

namespace odbcscanner {

DbmsQuirks::DbmsQuirks(OdbcConnection &conn, const std::map<std::string, std::unique_ptr<Value>> &user_quirks) {

	// Quirks assigned based on DBMS name reported by the driver

	switch (conn.driver) {
	case DbmsDriver::ORACLE:
		this->var_len_params_long_threshold_bytes = 4000;
		this->decimal_columns_precision_through_ard = true;
		this->integral_params_as_decimals = true;
		this->timestamp_columns_with_typename_date_as_date = true;
		this->enable_columns_binding = true;
		break;
	case DbmsDriver::MSSQL:
		this->var_len_params_long_threshold_bytes = 8000;
		this->decimal_columns_precision_through_ard = true;
		this->decimal_columns_as_ard_type = true;
		this->decimal_params_as_chars = true;
		this->time_params_as_ss_time2 = true;
		this->timestamp_max_fraction_precision = 7;
		this->timestamptz_params_as_ss_timestampoffset = true;
		break;
	case DbmsDriver::DB2:
		this->decimal_params_as_chars = true;
		this->decimal_columns_as_chars = true;
		this->enable_columns_binding = true;
		break;

	case DbmsDriver::MARIADB:
	case DbmsDriver::MYSQL:
		this->decimal_params_as_chars = true;
		this->decimal_columns_as_chars = true;
		break;
	case DbmsDriver::POSTGRESQL:
		break;
	case DbmsDriver::FIREBIRD:
		this->decimal_params_as_chars = true;
		this->decimal_columns_as_chars = true;
		break;

	case DbmsDriver::SNOWFLAKE:
		this->decimal_columns_precision_through_ard = true;
		this->decimal_params_as_chars = true;
		this->timestamp_params_as_sf_timestamp_ntz = true;
		break;
	case DbmsDriver::SPARK:
		this->decimal_params_as_chars = true;
		this->decimal_columns_as_chars = true;
		break;
	case DbmsDriver::CLICKHOUSE:
		this->decimal_params_as_chars = true;
		this->decimal_columns_as_chars = true;
		this->reset_stmt_before_execute = true;
		this->var_len_data_single_part = true;
		break;
	case DbmsDriver::FLIGTHSQL:
		this->decimal_columns_as_chars = true;
		break;
	case DbmsDriver::INFORMIX:
		this->var_len_params_long_threshold_bytes = 65478;
		break;

	case DbmsDriver::GENERIC:
		break;
		// default: no-op
	}

	// Quirks explicitly requested by user

	for (auto &en : user_quirks) {
		const std::unique_ptr<Value> &val = en.second;
		if (!val || val->IsNull()) {
			continue;
		}
		if (en.first == "decimal_columns_as_chars") {
			this->decimal_columns_as_chars = val->Get<bool>();
		} else if (en.first == "decimal_columns_precision_through_ard") {
			this->decimal_columns_precision_through_ard = val->Get<bool>();
		} else if (en.first == "decimal_columns_as_ard_type") {
			this->decimal_columns_as_ard_type = val->Get<bool>();
		} else if (en.first == "decimal_params_as_chars") {
			this->decimal_params_as_chars = val->Get<bool>();
		} else if (en.first == "integral_params_as_decimals") {
			this->integral_params_as_decimals = val->Get<bool>();
		} else if (en.first == "reset_stmt_before_execute") {
			this->reset_stmt_before_execute = val->Get<bool>();
		} else if (en.first == "time_params_as_ss_time2") {
			this->time_params_as_ss_time2 = val->Get<bool>();
		} else if (en.first == "timestamp_columns_as_timestamp_ns") {
			this->timestamp_columns_as_timestamp_ns = val->Get<bool>();
		} else if (en.first == "timestamp_columns_with_typename_date_as_date") {
			this->timestamp_columns_with_typename_date_as_date = val->Get<bool>();
		} else if (en.first == "timestamp_max_fraction_precision") {
			uint8_t num = val->Get<uint8_t>();
			if (num > 9) {
				throw ScannerException("Invalid value for user option 'timestamp_max_fraction_precision': " +
				                       std::to_string(num) + ", max value: 9");
			}
			this->timestamp_max_fraction_precision = num;
		} else if (en.first == "timestamp_params_as_sf_timestamp_ntz") {
			this->timestamp_params_as_sf_timestamp_ntz = val->Get<bool>();
		} else if (en.first == "timestamptz_params_as_ss_timestampoffset") {
			this->timestamptz_params_as_ss_timestampoffset = val->Get<bool>();
		} else if (en.first == "var_len_data_single_part") {
			this->var_len_data_single_part = val->Get<bool>();
		} else if (en.first == "var_len_params_long_threshold_bytes") {
			uint32_t num = val->Get<uint32_t>();
			this->var_len_params_long_threshold_bytes = num;
		} else if (en.first == "enable_columns_binding") {
			this->enable_columns_binding = val->Get<bool>();
		} else {
			throw ScannerException("Unsupported user option: '" + en.first + "'");
		}
	}
}

const std::vector<std::string> DbmsQuirks::AllNames() {
	std::vector<std::string> res;
	res.emplace_back("decimal_columns_as_chars");
	res.emplace_back("decimal_columns_precision_through_ard");
	res.emplace_back("decimal_columns_as_ard_type");
	res.emplace_back("decimal_params_as_chars");
	res.emplace_back("integral_params_as_decimals");
	res.emplace_back("reset_stmt_before_execute");
	res.emplace_back("time_params_as_ss_time2");
	res.emplace_back("timestamp_columns_as_timestamp_ns");
	res.emplace_back("timestamp_columns_with_typename_date_as_date");
	res.emplace_back("timestamp_max_fraction_precision");
	res.emplace_back("timestamp_params_as_sf_timestamp_ntz");
	res.emplace_back("timestamptz_params_as_ss_timestampoffset");
	res.emplace_back("var_len_data_single_part");
	res.emplace_back("var_len_params_long_threshold_bytes");
	res.emplace_back("enable_columns_binding");
	return res;
}

std::map<std::string, std::unique_ptr<Value>> DbmsQuirks::ExtractUserQuirks(FunctionBindInput &input) {
	std::map<std::string, std::unique_ptr<Value>> res;
	for (auto &name : DbmsQuirks::AllNames()) {
		auto val = FunctionArgs::GetNamedArg(input, name);
		res.emplace(name, std::move(val));
	}
	return res;
}

} // namespace odbcscanner
