#include "capi_odbc_scanner.h"

DUCKDB_EXTENSION_ENTRYPOINT(duckdb_connection connection, duckdb_extension_info info,
                            struct duckdb_extension_access *access) {
	(void)info;
	(void)access;

	{
		duckdb_state state = odbc_connect_register(connection);
		if (state != DuckDBSuccess) {
			return false;
		}
	}

	{
		duckdb_state state = odbc_close_register(connection);
		if (state != DuckDBSuccess) {
			return false;
		}
	}

	{
		duckdb_state state = odbc_query_register(connection);
		if (state != DuckDBSuccess) {
			return false;
		}
	}

	{
		duckdb_state state = odbc_params_register(connection);
		if (state != DuckDBSuccess) {
			return false;
		}
	}

	return true;
}
