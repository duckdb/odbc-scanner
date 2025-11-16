#include "capi_entry_point.hpp"

// DUCKDB_EXTENSION_ENTRYPOINT unfolded below

duckdb_ext_api_v1 duckdb_ext_api;

bool odbc_scanner_init_c_api(duckdb_extension_info info, struct duckdb_extension_access *access) {
	duckdb_ext_api_v1 *res = (duckdb_ext_api_v1 *)access->get_api(info, "v"
	                                                                    "1"
	                                                                    "."
	                                                                    "2"
	                                                                    "."
	                                                                    "0");

	if (!res) {
		return false;
	};

	duckdb_ext_api = *res;
	duckdb_database *db = access->get_database(info);
	duckdb_connection conn;

	if (duckdb_connect(*db, &conn) == DuckDBError) {
		access->set_error(info, "Failed to open connection to database");
		return false;
	}

	bool init_result = initiaize_odbc_scanner(conn, info, access);

	duckdb_disconnect(&conn);

	return init_result;
}
