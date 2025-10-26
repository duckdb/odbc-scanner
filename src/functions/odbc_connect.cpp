#include "odbc_scanner.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "capi_pointers.hpp"
#include "connection.hpp"
#include "diagnostics.hpp"
#include "make_unique.hpp"
#include "odbc_api.hpp"
#include "registries.hpp"
#include "scanner_exception.hpp"
#include "strings.hpp"
#include "types.hpp"

DUCKDB_EXTENSION_EXTERN

static const std::string ODBCSCANNER_DEBUG_CONN_STRING_ENV_VAR = "ODBCSCANNER_DEBUG_CONN_STRING_ENV_VAR";

static void odbc_connect_function(duckdb_function_info info, duckdb_data_chunk input, duckdb_vector output) noexcept;

namespace odbcscanner {

static void Connect(duckdb_function_info info, duckdb_data_chunk input, duckdb_vector output) {
	(void)info;

	auto arg = Types::ExtractFunctionArg<std::string>(input, 0);
	if (arg.second) {
		throw ScannerException("'odbc_connect' error: specified URL argument must be not NULL");
	}

	std::string url = arg.first;

	// Env var fetch is not thread-safe, should be used only for debugging,
	// ideally this logic should be moved into SQLLogic test runner.
	if (url.rfind(ODBCSCANNER_DEBUG_CONN_STRING_ENV_VAR, 0) == 0 && url.find(";") == std::string::npos) {
		std::vector<std::string> parts = Strings::Split(url, '=');
		if (parts.size() == 2 && ODBCSCANNER_DEBUG_CONN_STRING_ENV_VAR == parts.at(0)) {
			std::string &var_name = parts.at(1);
			char *var = std::getenv(var_name.c_str());
			url = var != nullptr ? std::string(var) : "Driver={DuckDB Driver};";
		}
	}

	auto oc_ptr = std_make_unique<OdbcConnection>(url);

	int64_t *result_data = reinterpret_cast<int64_t *>(duckdb_vector_get_data(output));
	result_data[0] = ConnectionsRegistry::Add(std::move(oc_ptr));
}

void OdbcConnectFunction::Register(duckdb_connection conn) {
	auto fun = ScalarFunctionPtr(duckdb_create_scalar_function(), ScalarFunctionDeleter);
	duckdb_scalar_function_set_name(fun.get(), "odbc_connect");

	// parameters and return
	auto varchar_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_VARCHAR), LogicalTypeDeleter);
	auto bigint_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_BIGINT), LogicalTypeDeleter);
	duckdb_scalar_function_add_parameter(fun.get(), varchar_type.get());
	duckdb_scalar_function_set_return_type(fun.get(), bigint_type.get());

	// callbacks
	duckdb_scalar_function_set_function(fun.get(), odbc_connect_function);

	// options
	duckdb_scalar_function_set_volatile(fun.get());
	duckdb_scalar_function_set_special_handling(fun.get());

	// register and cleanup
	duckdb_state state = duckdb_register_scalar_function(conn, fun.get());

	if (state != DuckDBSuccess) {
		throw ScannerException("'odbc_connect' function registration failed");
	}
}

} // namespace odbcscanner

static void odbc_connect_function(duckdb_function_info info, duckdb_data_chunk input, duckdb_vector output) noexcept {
	try {
		odbcscanner::Connect(info, input, output);
	} catch (std::exception &e) {
		duckdb_scalar_function_set_error(info, e.what());
	}
}
