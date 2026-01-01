#include "odbc_scanner.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "capi_pointers.hpp"
#include "dbms_quirks.hpp"
#include "defer.hpp"
#include "diagnostics.hpp"
#include "make_unique.hpp"
#include "params.hpp"
#include "registries.hpp"
#include "scanner_exception.hpp"
#include "types.hpp"

DUCKDB_EXTENSION_EXTERN

static void odbc_bind_params_function(duckdb_function_info info, duckdb_data_chunk input,
                                      duckdb_vector output) noexcept;

namespace odbcscanner {

static void BindParams(duckdb_function_info info, duckdb_data_chunk input, duckdb_vector output) {
	(void)info;

	auto conn_arg = Types::ExtractFunctionArg<int64_t>(input, 0);
	if (conn_arg.second) {
		throw ScannerException("'odbc_bind_params' error: specified ODBC connection must be not NULL");
	}
	int64_t conn_id = conn_arg.first;
	auto conn_ptr = ConnectionsRegistry::Remove(conn_id);

	if (conn_ptr.get() == nullptr) {
		throw ScannerException("'odbc_bind_params' error: open ODBC connection not found, id: " +
		                       std::to_string(conn_id));
	}

	// Return the connection to registry at the end of the block
	auto deferred_conn = Defer([&conn_ptr] { ConnectionsRegistry::Add(std::move(conn_ptr)); });

	OdbcConnection &conn = *conn_ptr;

	auto handle_arg = Types::ExtractFunctionArg<int64_t>(input, 1);
	if (handle_arg.second) {
		throw ScannerException("'odbc_bind_params' error: specified parameters handle argument must be not NULL");
	}
	int64_t params_handle = handle_arg.first;
	auto params_ptr = ParamsRegistry::Remove(params_handle);
	if (params_ptr.get() == nullptr) {
		throw ScannerException("'odbc_bind_params' error: specified parameters handle not found, ID: " +
		                       std::to_string(params_handle));
	}
	auto deferred_params = Defer([&params_ptr] { ParamsRegistry::Add(std::move(params_ptr)); });
	params_ptr->clear();

	// currently do not support user quirks in 'odbc_bind_params'
	DbmsQuirks quirks(conn, std::map<std::string, ValuePtr>());
	std::vector<ScannerValue> params = Params::Extract(quirks, input, 2);

	for (ScannerValue &p : params) {
		params_ptr->emplace_back(std::move(p));
	}

	int64_t *result_data = reinterpret_cast<int64_t *>(duckdb_vector_get_data(output));
	result_data[0] = params_handle;
}

void OdbcBindParamsFunction::Register(duckdb_connection conn) {
	auto fun = ScalarFunctionPtr(duckdb_create_scalar_function(), ScalarFunctionDeleter);
	duckdb_scalar_function_set_name(fun.get(), "odbc_bind_params");

	// parameters and return
	auto bigint_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_BIGINT), LogicalTypeDeleter);
	auto any_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_ANY), LogicalTypeDeleter);
	duckdb_scalar_function_add_parameter(fun.get(), bigint_type.get());
	duckdb_scalar_function_add_parameter(fun.get(), bigint_type.get());
	duckdb_scalar_function_add_parameter(fun.get(), any_type.get());
	duckdb_scalar_function_set_return_type(fun.get(), bigint_type.get());

	// callbacks
	duckdb_scalar_function_set_function(fun.get(), odbc_bind_params_function);

	// options
	duckdb_scalar_function_set_volatile(fun.get());
	duckdb_scalar_function_set_special_handling(fun.get());

	// register and cleanup
	duckdb_state state = duckdb_register_scalar_function(conn, fun.get());

	if (state != DuckDBSuccess) {
		throw ScannerException("'odbc_bind_params' function registration failed");
	}
}

} // namespace odbcscanner

static void odbc_bind_params_function(duckdb_function_info info, duckdb_data_chunk input,
                                      duckdb_vector output) noexcept {
	try {
		odbcscanner::BindParams(info, input, output);
	} catch (std::exception &e) {
		duckdb_scalar_function_set_error(info, e.what());
	}
}
