#include "capi_odbc_scanner.h"
#include "odbc_connection.hpp"

#include <sql.h>
#include <sqlext.h>
#include <string>

DUCKDB_EXTENSION_EXTERN

void odbc_close_function(duckdb_function_info info, duckdb_data_chunk input, duckdb_vector output) noexcept {
	(void)info;

	idx_t input_size = duckdb_data_chunk_get_size(input);
	if (input_size != 1) {
		// todo: throw
		return;
	}

	duckdb_vector ptr_vec = duckdb_data_chunk_get_vector(input, 0);
	int64_t *ptr_data = reinterpret_cast<int64_t *>(duckdb_vector_get_data(ptr_vec));
	int64_t ptr_num = ptr_data[0];
	OdbcConnection *conn = reinterpret_cast<OdbcConnection *>(ptr_num);
	delete conn;

	duckdb_vector_ensure_validity_writable(output);
	uint64_t *result_validity = duckdb_vector_get_validity(output);
	duckdb_validity_set_row_invalid(result_validity, 0);
}

void odbc_close_register(duckdb_connection conn) /* noexcept */ {
	duckdb_scalar_function fun = duckdb_create_scalar_function();
	duckdb_scalar_function_set_name(fun, "odbc_close");

	// parameters and return
	duckdb_logical_type bigint_type = duckdb_create_logical_type(DUCKDB_TYPE_BIGINT);
	duckdb_logical_type varchar_type = duckdb_create_logical_type(DUCKDB_TYPE_VARCHAR);
	duckdb_scalar_function_add_parameter(fun, bigint_type);
	duckdb_scalar_function_set_return_type(fun, varchar_type);
	duckdb_destroy_logical_type(&varchar_type);
	duckdb_destroy_logical_type(&bigint_type);

	// callbacks
	duckdb_scalar_function_set_function(fun, odbc_close_function);

	// register and cleanup
	duckdb_register_scalar_function(conn, fun);
	duckdb_destroy_scalar_function(&fun);
}
