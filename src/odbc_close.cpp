#include "capi_odbc_scanner.h"
#include "capi_pointers.hpp"
#include "diagnostics.hpp"
#include "odbc_connection.hpp"
#include "scanner_exception.hpp"
#include "types/type_bigint.hpp"

#include <sql.h>
#include <sqlext.h>
#include <string>

DUCKDB_EXTENSION_EXTERN

static void odbc_close_function(duckdb_function_info info, duckdb_data_chunk input, duckdb_vector output) noexcept;

namespace odbcscanner {

static void Close(duckdb_function_info info, duckdb_data_chunk input, duckdb_vector output) {
	(void)info;

	auto arg = ExtractBigIntFunctionArg(input, 0);
	if (arg.second) {
		throw ScannerException("'odbc_close' error: specified ODBC connection argument must be not NULL");
	}
	auto *conn = reinterpret_cast<OdbcConnection *>(arg.first);
	delete conn;

	duckdb_vector_ensure_validity_writable(output);
	uint64_t *result_validity = duckdb_vector_get_validity(output);
	duckdb_validity_set_row_invalid(result_validity, 0);
}

static duckdb_state Register(duckdb_connection conn) {
	auto fun = ScalarFunctionPtr(duckdb_create_scalar_function(), ScalarFunctionDeleter);
	duckdb_scalar_function_set_name(fun.get(), "odbc_close");

	// parameters and return
	auto bigint_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_BIGINT), LogicalTypeDeleter);
	auto varchar_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_VARCHAR), LogicalTypeDeleter);
	duckdb_scalar_function_add_parameter(fun.get(), bigint_type.get());
	duckdb_scalar_function_set_return_type(fun.get(), varchar_type.get());

	// callbacks
	duckdb_scalar_function_set_function(fun.get(), odbc_close_function);

	// options
	duckdb_scalar_function_set_special_handling(fun.get());
	duckdb_scalar_function_set_volatile(fun.get());

	// register and cleanup
	duckdb_state state = duckdb_register_scalar_function(conn, fun.get());

	return state;
}

} // namespace odbcscanner

static void odbc_close_function(duckdb_function_info info, duckdb_data_chunk input, duckdb_vector output) noexcept {
	try {
		odbcscanner::Close(info, input, output);
	} catch (std::exception &e) {
		duckdb_scalar_function_set_error(info, e.what());
	}
}

duckdb_state odbc_close_register(duckdb_connection conn) /* noexcept */ {
	try {
		return odbcscanner::Register(conn);
	} catch (std::exception &e) {
		(void)e;
		return DuckDBError;
	}
}
