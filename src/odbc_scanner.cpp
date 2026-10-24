#include "odbc_scanner.hpp"

#include <stdexcept>
#include <string>

#include "capi_entry_point.hpp"
#include "registries.hpp"
#include "scanner_exception.hpp"

namespace odbcscanner {

static void Initialize(Extension &extension, Context &context) {
	Factory &factory = context.GetFactory();
	Registries::Initialize();
	OdbcBeginTransactionFunction::Register(extension, factory);
	OdbcCancelQueryFunction::Register(extension, factory);
	OdbcCloseFunction::Register(extension, factory);
	OdbcCommitFunction::Register(extension, factory);
	OdbcConnectFunction::Register(extension, factory);
	OdbcCopyFunction::Register(extension, factory);
	OdbcListDataSourcesFunction::Register(extension, factory);
	OdbcListDriversFunction::Register(extension, factory);
	OdbcQueryFunction::Register(extension, factory);
	OdbcRollbackFunction::Register(extension, factory);
}

} // namespace odbcscanner

void initiaize_odbc_scanner(duckdb::cxx::Extension &extension, duckdb::cxx::Context &context) {
	try {
		odbcscanner::Initialize(extension, context);
	} catch (std::exception &e) {
		throw duckdb::cxx::InvalidInputException("ODBC Scanner initialization failed: " + std::string(e.what()));
	}
}
