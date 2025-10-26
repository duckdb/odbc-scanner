#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "duckdb_extension.h"

bool initiaize_odbc_scanner(duckdb_connection connection, duckdb_extension_info info,
                            struct duckdb_extension_access *access);

#ifdef __cplusplus
}
#endif // __cplusplus
