#pragma once

#include "duckdb_extension_api.hpp"

namespace odbcscanner {

struct OdbcBindParamsFunction {
	static void Register(duckdb_connection connection);
};

struct OdbcCloseFunction {
	static void Register(duckdb_connection connection);
};

struct OdbcConnectFunction {
	static void Register(duckdb_connection connection);
};

struct OdbcCreateParamsFunction {
	static void Register(duckdb_connection connection);
};

struct OdbcListDataSourcesFunction {
	static void Register(duckdb_connection connection);
};

struct OdbcListDriversFunction {
	static void Register(duckdb_connection connection);
};

struct OdbcQueryFunction {
	static void Register(duckdb_connection connection);
};

} // namespace odbcscanner