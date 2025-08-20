#include "capi_odbc_scanner.h"
#include "capi_pointers.hpp"
#include "defer.hpp"
#include "diagnostics.hpp"
#include "make_unique.hpp"
#include "params.hpp"
#include "registries.hpp"
#include "scanner_exception.hpp"
#include "types/type_bigint.hpp"

#include <string>
#include <vector>

DUCKDB_EXTENSION_EXTERN

static void odbc_bind_params_function(duckdb_function_info info, duckdb_data_chunk input,
                                      duckdb_vector output) noexcept;

namespace odbcscanner {

static void BindParams(duckdb_function_info info, duckdb_data_chunk input, duckdb_vector output) {
	(void)info;

	auto handle_arg = ExtractBigIntFunctionArg(input, 0);
	if (handle_arg.second) {
		throw ScannerException("'odbc_bind_params' error: specified parameters handle argument must be not NULL");
	}
	int64_t params_handle = handle_arg.first;
	auto params_ptr = RemoveParamsFromRegistry(params_handle);
	if (params_ptr.get() == nullptr) {
		throw ScannerException("'odbc_bind_params' error: specified parameter handle not found, ID: " +
		                       std::to_string(params_handle));
	}
	auto deferred = Defer([&params_ptr] { AddParamsToRegistry(std::move(params_ptr)); });
	params_ptr->clear();

	std::vector<ScannerParam> params = ExtractStructParamsFromChunk(input, 1);

	for (ScannerParam &p : params) {
		params_ptr->emplace_back(std::move(p));
	}

	int64_t *result_data = reinterpret_cast<int64_t *>(duckdb_vector_get_data(output));
	result_data[0] = params_handle;
}

static duckdb_state Register(duckdb_connection conn) {
	auto fun = ScalarFunctionPtr(duckdb_create_scalar_function(), ScalarFunctionDeleter);
	duckdb_scalar_function_set_name(fun.get(), "odbc_bind_params");

	// parameters and return
	auto bigint_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_BIGINT), LogicalTypeDeleter);
	auto any_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_ANY), LogicalTypeDeleter);
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

	return state;
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

duckdb_state odbc_bind_params_register(duckdb_connection conn) /* noexcept */ {
	try {
		return odbcscanner::Register(conn);
	} catch (std::exception &e) {
		(void)e;
		return DuckDBError;
	}
}