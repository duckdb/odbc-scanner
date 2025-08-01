#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "duckdb_extension.h"

duckdb_state odbc_connect_register(duckdb_connection connection);

duckdb_state odbc_close_register(duckdb_connection connection);

duckdb_state odbc_query_register(duckdb_connection connection);

#ifdef __cplusplus
}
#endif // __cplusplus
