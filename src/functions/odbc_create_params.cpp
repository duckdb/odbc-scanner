#include "capi_odbc_scanner.h"
#include "capi_pointers.hpp"
#include "diagnostics.hpp"
#include "make_unique.hpp"
#include "params.hpp"
#include "registries.hpp"
#include "scanner_exception.hpp"

#include <string>
#include <vector>

DUCKDB_EXTENSION_EXTERN

static void odbc_create_params_function(duckdb_function_info info, duckdb_data_chunk input,
                                        duckdb_vector output) noexcept;

namespace odbcscanner {

static void CreateParams(duckdb_function_info info, duckdb_data_chunk input, duckdb_vector output) {
	(void)info;
	(void)input;

	auto params_ptr = std_make_unique<std::vector<ScannerParam>>();
	int64_t *result_data = reinterpret_cast<int64_t *>(duckdb_vector_get_data(output));
	result_data[0] = AddParamsToRegistry(std::move(params_ptr));
}

static duckdb_state Register(duckdb_connection conn) {
	auto fun = ScalarFunctionPtr(duckdb_create_scalar_function(), ScalarFunctionDeleter);
	duckdb_scalar_function_set_name(fun.get(), "odbc_create_params");

	// parameters and return
	auto bigint_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_BIGINT), LogicalTypeDeleter);
	duckdb_scalar_function_set_return_type(fun.get(), bigint_type.get());

	// callbacks
	duckdb_scalar_function_set_function(fun.get(), odbc_create_params_function);

	// options
	duckdb_scalar_function_set_volatile(fun.get());

	// register and cleanup
	duckdb_state state = duckdb_register_scalar_function(conn, fun.get());

	return state;
}

} // namespace odbcscanner

static void odbc_create_params_function(duckdb_function_info info, duckdb_data_chunk input,
                                        duckdb_vector output) noexcept {
	try {
		odbcscanner::CreateParams(info, input, output);
	} catch (std::exception &e) {
		duckdb_scalar_function_set_error(info, e.what());
	}
}

duckdb_state odbc_create_params_register(duckdb_connection conn) /* noexcept */ {
	try {
		return odbcscanner::Register(conn);
	} catch (std::exception &e) {
		(void)e;
		return DuckDBError;
	}
}
