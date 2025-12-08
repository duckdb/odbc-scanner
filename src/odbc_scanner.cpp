#include "odbc_scanner.hpp"

#include <stdexcept>
#include <string>

#include "capi_pointers.hpp"
#include "capi_entry_point.hpp"
#include "registries.hpp"
#include "scanner_exception.hpp"

namespace odbcscanner {

static void Initialize(duckdb_connection connection, duckdb_extension_info, duckdb_extension_access *) {
	Registries::Initialize();
	OdbcBindParamsFunction::Register(connection);
	OdbcCloseFunction::Register(connection);
	OdbcConnectFunction::Register(connection);
	OdbcCreateParamsFunction::Register(connection);
	OdbcListDataSourcesFunction::Register(connection);
	OdbcListDriversFunction::Register(connection);
	OdbcQueryFunction::Register(connection);
}

} // namespace odbcscanner

bool initiaize_odbc_scanner(duckdb_connection connection, duckdb_extension_info info,
                            duckdb_extension_access *access) noexcept {
	try {
		odbcscanner::Initialize(connection, info, access);
		return true;
	} catch (std::exception &e) {
		std::string msg = "ODBC Scanner initialization failed: " + std::string(e.what());
		access->set_error(info, msg.c_str());
		return false;
	}
}
