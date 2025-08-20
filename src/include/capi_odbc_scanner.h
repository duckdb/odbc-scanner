#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "duckdb_extension.h"

void initialize_connection_registry();

duckdb_state odbc_connect_register(duckdb_connection connection);

duckdb_state odbc_close_register(duckdb_connection connection);

duckdb_state odbc_query_register(duckdb_connection connection);

duckdb_state odbc_create_params_register(duckdb_connection connection);

duckdb_state odbc_bind_params_register(duckdb_connection connection);

#ifdef __cplusplus
}
#endif // __cplusplus
