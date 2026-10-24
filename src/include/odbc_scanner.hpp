#pragma once

#include "duckdb_extension_api.hpp"

namespace odbcscanner {

struct OdbcBeginTransactionFunction {
	static void Register(Extension &extension, Factory &factory);
};

struct OdbcCancelQueryFunction {
	static void Register(Extension &extension, Factory &factory);
};

struct OdbcCloseFunction {
	static void Register(Extension &extension, Factory &factory);
};

struct OdbcCommitFunction {
	static void Register(Extension &extension, Factory &factory);
};

struct OdbcConnectFunction {
	static void Register(Extension &extension, Factory &factory);
};

struct OdbcCopyFunction {
	static void Register(Extension &extension, Factory &factory);
};

struct OdbcListDataSourcesFunction {
	static void Register(Extension &extension, Factory &factory);
};

struct OdbcListDriversFunction {
	static void Register(Extension &extension, Factory &factory);
};

struct OdbcQueryFunction {
	static void Register(Extension &extension, Factory &factory);
};

struct OdbcRollbackFunction {
	static void Register(Extension &extension, Factory &factory);
};

} // namespace odbcscanner