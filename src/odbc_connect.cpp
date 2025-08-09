#include "capi_odbc_scanner.h"
#include "capi_pointers.hpp"
#include "diagnostics.hpp"
#include "make_unique.hpp"
#include "odbc_connection.hpp"
#include "scanner_exception.hpp"
#include "types/type_varchar.hpp"

#include <memory>
#include <sql.h>
#include <sqlext.h>
#include <string>

DUCKDB_EXTENSION_EXTERN

static void odbc_connect_function(duckdb_function_info info, duckdb_data_chunk input, duckdb_vector output) noexcept;

namespace odbcscanner {

static void Connect(duckdb_function_info info, duckdb_data_chunk input, duckdb_vector output) {
	(void)info;

	auto arg = ExtractVarcharFunctionArg(input, 0);
	if (arg.second) {
		throw ScannerException("'odbc_connect' error: specified URL argument must be not NULL");
	}

	auto oc_ptr = std_make_unique<OdbcConnection>(arg.first);

	int64_t *result_data = reinterpret_cast<int64_t *>(duckdb_vector_get_data(output));
	result_data[0] = reinterpret_cast<int64_t>(oc_ptr.release());
}

static duckdb_state Register(duckdb_connection conn) {
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

	// register and cleanup
	duckdb_state state = duckdb_register_scalar_function(conn, fun.get());

	return state;
}

} // namespace odbcscanner

static void odbc_connect_function(duckdb_function_info info, duckdb_data_chunk input, duckdb_vector output) noexcept {
	try {
		odbcscanner::Connect(info, input, output);
	} catch (std::exception &e) {
		duckdb_scalar_function_set_error(info, e.what());
	}
}

duckdb_state odbc_connect_register(duckdb_connection conn) /* noexcept */ {
	try {
		return odbcscanner::Register(conn);
	} catch (std::exception &e) {
		(void)e;
		return DuckDBError;
	}
}
