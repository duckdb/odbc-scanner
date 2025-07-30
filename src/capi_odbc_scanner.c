#include "capi_odbc_scanner.h"

DUCKDB_EXTENSION_ENTRYPOINT(duckdb_connection connection, duckdb_extension_info info,
                            struct duckdb_extension_access *access) {
	(void)info;
	(void)access;

	odbc_connect_register(connection);
	odbc_close_register(connection);
	odbc_query_register(connection);

	return true;
}
