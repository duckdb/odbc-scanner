#include "capi_odbc_scanner.h"
#include "odbc_connection.hpp"

#include <memory>
#include <sql.h>
#include <sqlext.h>
#include <string>

DUCKDB_EXTENSION_EXTERN

// todo: error handling

void odbc_connect_function(duckdb_function_info info, duckdb_data_chunk input, duckdb_vector output) noexcept {
	(void)info;

	idx_t input_size = duckdb_data_chunk_get_size(input);
	if (input_size != 1) {
		// todo: throw
		return;
	}

	duckdb_vector url_vec = duckdb_data_chunk_get_vector(input, 0);
	duckdb_string_t *url_data = reinterpret_cast<duckdb_string_t *>(duckdb_vector_get_data(url_vec));
	duckdb_string_t url_str_t = url_data[0];
	const char *url_cstr = duckdb_string_t_data(&url_str_t);
	uint32_t url_len = duckdb_string_t_length(url_str_t);
	std::string url(url_cstr, url_len);

	auto oc_ptr = std::unique_ptr<OdbcConnection>(new OdbcConnection(url));

	int64_t *result_data = reinterpret_cast<int64_t *>(duckdb_vector_get_data(output));
	result_data[0] = reinterpret_cast<int64_t>(oc_ptr.release());
}

void odbc_connect_register(duckdb_connection conn) /* noexcept */ {
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
	duckdb_register_scalar_function(conn, fun);
	duckdb_destroy_scalar_function(&fun);
}