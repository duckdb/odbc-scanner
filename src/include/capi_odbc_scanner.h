#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "duckdb_extension.h"

void odbc_connect_register(duckdb_connection connection);

void odbc_close_register(duckdb_connection connection);

void odbc_query_register(duckdb_connection connection);

#ifdef __cplusplus
}
#endif // __cplusplus