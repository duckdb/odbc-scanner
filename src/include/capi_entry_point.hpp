#pragma once

#include "duckdb_extension_api.hpp"

extern "C" DUCKDB_CAPI_ENTRY_VISIBILITY bool odbc_scanner_init_c_api(duckdb_extension_info info,
                                                                     struct duckdb_extension_access *access);

bool initiaize_odbc_scanner(duckdb_connection connection, duckdb_extension_info info,
                            struct duckdb_extension_access *access) noexcept;
