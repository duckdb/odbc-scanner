#include "odbc_scanner.hpp"

#include <cstdint>
#include <string>

#include "capi_pointers.hpp"
#include "defer.hpp"
#include "diagnostics.hpp"
#include "registries.hpp"
#include "scanner_exception.hpp"
#include "types.hpp"

DUCKDB_EXTENSION_EXTERN

static void odbc_cancel_query_function(duckdb_function_info info, duckdb_data_chunk input,
                                       duckdb_vector output) noexcept;

namespace odbcscanner {

static void CancelQuery(duckdb_function_info info, duckdb_data_chunk input, duckdb_vector output) {
	(void)info;

	auto conn_arg = Types::ExtractFunctionArg<int64_t>(input, 0);
	if (conn_arg.second) {
		throw ScannerException("'odbc_cancel_query' error: specified ODBC connection must be not NULL");
	}
	int64_t conn_id = conn_arg.first;
	HSTMT stmt = CancellationRegistry::Get(conn_id);

	bool success = false;
	if (stmt != nullptr) {
		// the handle may or may not be valid
		SQLRETURN ret = SQLCancelHandle(SQL_HANDLE_STMT, stmt);
		if (SQL_SUCCEEDED(ret)) {
			success = true;
		}
	}

	bool *result_data = reinterpret_cast<bool *>(duckdb_vector_get_data(output));
	result_data[0] = success;
}

void OdbcCancelQueryFunction::Register(duckdb_connection conn) {
	auto fun = ScalarFunctionPtr(duckdb_create_scalar_function(), ScalarFunctionDeleter);
	duckdb_scalar_function_set_name(fun.get(), "odbc_cancel_query");

	// parameters and return
	auto bigint_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_BIGINT), LogicalTypeDeleter);
	auto boolean_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_BOOLEAN), LogicalTypeDeleter);
	duckdb_scalar_function_add_parameter(fun.get(), bigint_type.get());
	duckdb_scalar_function_set_return_type(fun.get(), boolean_type.get());

	// callbacks
	duckdb_scalar_function_set_function(fun.get(), odbc_cancel_query_function);

	// options
	duckdb_scalar_function_set_volatile(fun.get());
	duckdb_scalar_function_set_special_handling(fun.get());

	// register and cleanup
	duckdb_state state = duckdb_register_scalar_function(conn, fun.get());

	if (state != DuckDBSuccess) {
		throw ScannerException("'odbc_cancel_query' function registration failed");
	}
}

} // namespace odbcscanner

static void odbc_cancel_query_function(duckdb_function_info info, duckdb_data_chunk input,
                                       duckdb_vector output) noexcept {
	try {
		odbcscanner::CancelQuery(info, input, output);
	} catch (std::exception &e) {
		duckdb_scalar_function_set_error(info, e.what());
	}
}
