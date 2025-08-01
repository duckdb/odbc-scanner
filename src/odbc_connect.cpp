#include "capi_odbc_scanner.h"
#include "common.hpp"
#include "make_unique.hpp"
#include "odbc_connection.hpp"
#include "scanner_exception.hpp"

#include <memory>
#include <sql.h>
#include <sqlext.h>
#include <string>

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

static void Connect(duckdb_function_info info, duckdb_data_chunk input, duckdb_vector output) {
	(void)info;

	CheckChunkRowsCount(input);
	std::string url = ExtractStringFromChunk(input, 0);

	auto oc_ptr = std_make_unique<OdbcConnection>(url);

	int64_t *result_data = reinterpret_cast<int64_t *>(duckdb_vector_get_data(output));
	result_data[0] = reinterpret_cast<int64_t>(oc_ptr.release());
}

} // namespace odbcscanner

void odbc_connect_function(duckdb_function_info info, duckdb_data_chunk input, duckdb_vector output) noexcept {
	try {
		odbcscanner::Connect(info, input, output);
	} catch (std::exception &e) {
		duckdb_scalar_function_set_error(info, e.what());
	}
}

duckdb_state odbc_connect_register(duckdb_connection conn) /* noexcept */ {
	duckdb_scalar_function fun = duckdb_create_scalar_function();
	duckdb_scalar_function_set_name(fun, "odbc_connect");

	// parameters and return
	duckdb_logical_type varchar_type = duckdb_create_logical_type(DUCKDB_TYPE_VARCHAR);
	duckdb_logical_type bigint_type = duckdb_create_logical_type(DUCKDB_TYPE_BIGINT);
	duckdb_scalar_function_add_parameter(fun, varchar_type);
	duckdb_scalar_function_set_return_type(fun, bigint_type);
	duckdb_destroy_logical_type(&bigint_type);
	duckdb_destroy_logical_type(&varchar_type);

	// callbacks
	duckdb_scalar_function_set_function(fun, odbc_connect_function);

	// register and cleanup
	duckdb_state state = duckdb_register_scalar_function(conn, fun);
	duckdb_destroy_scalar_function(&fun);

	return state;
}
