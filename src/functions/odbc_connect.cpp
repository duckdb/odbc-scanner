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

static void AppendUsernameAndPassword(duckdb_data_chunk input, std::string &conn_str) {
	auto username_pair = Types::ExtractFunctionArg<std::string>(input, 1);
	if (username_pair.second) {
		throw ScannerException("'odbc_connect' error: specified username argument must be not NULL");
	}
	std::string username = username_pair.first;

	auto password_pair = Types::ExtractFunctionArg<std::string>(input, 2);
	if (password_pair.second) {
		throw ScannerException("'odbc_connect' error: specified password argument must be not NULL");
	}
	std::string password = password_pair.first;

	std::string conn_str_upper = Strings::ToUpper(conn_str);
	if (conn_str_upper.find("UID") != std::string::npos) {
		throw ScannerException("'odbc_connect' error: username (UID) cannot be specified in both connection string and "
		                       "a separate argument");
	}
	if (conn_str_upper.find("PWD") != std::string::npos) {
		throw ScannerException("'odbc_connect' error: password (PWD) cannot be specified in both connection string and "
		                       "a separate argument");
	}

	if (conn_str.length() == 0 || conn_str.at(conn_str.length() - 1) != ';') {
		conn_str.append(";");
	}
	conn_str.append("UID=");
	conn_str.append(username);
	conn_str.append(";");

	conn_str.append("PWD=");
	conn_str.append(password);
	conn_str.append(";");
}

static void Connect(duckdb_function_info info, duckdb_data_chunk input, duckdb_vector output) {
	(void)info;

	idx_t args_count = duckdb_data_chunk_get_column_count(input);

	if (args_count != 1 && args_count != 3) {
		throw ScannerException(
		    "'odbc_connect' error: invalid number of arguments specified, count: " + std::to_string(args_count) +
		    ", supported arguments: 'odbc_connect(conn_string: VARCHAR)', 'odbc_connect(conn_string: "
		    "VARCHAR, username: VARCHAR, password: VARCHAR)'");
	}

	auto conn_str_pair = Types::ExtractFunctionArg<std::string>(input, 0);
	if (conn_str_pair.second) {
		throw ScannerException("'odbc_connect' error: specified connection string argument must be not NULL");
	}
	std::string conn_str = conn_str_pair.first;

	if (args_count == 3) {
		AppendUsernameAndPassword(input, conn_str);
	}

	// Env var fetch is not thread-safe, should be used only for debugging,
	// ideally this logic should be moved into SQLLogic test runner.
	if (conn_str.rfind(ODBCSCANNER_DEBUG_CONN_STRING_ENV_VAR, 0) == 0 && conn_str.find(";") == std::string::npos) {
		std::vector<std::string> parts = Strings::Split(conn_str, '=');
		if (parts.size() == 2 && ODBCSCANNER_DEBUG_CONN_STRING_ENV_VAR == parts.at(0)) {
			std::string &var_name = parts.at(1);
			char *var = std::getenv(var_name.c_str());
			conn_str = var != nullptr ? std::string(var) : "Driver={DuckDB Driver};";
		}
	}

	auto oc_ptr = std_make_unique<OdbcConnection>(conn_str);

	int64_t *result_data = reinterpret_cast<int64_t *>(duckdb_vector_get_data(output));
	result_data[0] = ConnectionsRegistry::Add(std::move(oc_ptr));
}

void OdbcConnectFunction::Register(duckdb_connection conn) {
	auto fun = ScalarFunctionPtr(duckdb_create_scalar_function(), ScalarFunctionDeleter);
	duckdb_scalar_function_set_name(fun.get(), "odbc_connect");

	// parameters and return
	auto varchar_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_VARCHAR), LogicalTypeDeleter);
	auto bigint_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_BIGINT), LogicalTypeDeleter);
	duckdb_scalar_function_set_varargs(fun.get(), varchar_type.get());
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
