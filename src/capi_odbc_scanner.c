#include "capi_odbc_scanner.h"

DUCKDB_EXTENSION_ENTRYPOINT(duckdb_connection connection, duckdb_extension_info info,
                            struct duckdb_extension_access *access) {

	odbc_query_register(connection);

	return true;
}
