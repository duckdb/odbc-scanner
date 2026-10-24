#include "odbc_scanner.hpp"

#include <cstdint>
#include <string>

#include "function_args.hpp"
#include "defer.hpp"
#include "diagnostics.hpp"
#include "registries.hpp"
#include "scanner_exception.hpp"
#include "types.hpp"

namespace odbcscanner {

static void Rollback(ScalarFunction::ExecInput &input) {
	Vector output = input.GetResult();

	auto conn_arg = Types::ExtractFunctionArg<int64_t>(input, 0);
	if (conn_arg.second) {
		throw ScannerException("'odbc_rollback' error: specified ODBC connection must be not NULL");
	}
	int64_t conn_id = conn_arg.first;
	auto conn_ptr = ConnectionsRegistry::Remove(conn_id);

	if (conn_ptr.get() == nullptr) {
		throw ScannerException("'odbc_rollback' error: open ODBC connection not found, id: " + std::to_string(conn_id));
	}

	// Return the connection to registry at the end of the block
	auto deferred_conn = Defer([&conn_ptr] { ConnectionsRegistry::Add(std::move(conn_ptr)); });

	OdbcConnection &conn = *conn_ptr;

	SQLRETURN ret = SQLEndTran(SQL_HANDLE_DBC, conn.dbc, SQL_ROLLBACK);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(conn.dbc, SQL_HANDLE_DBC);
		throw ScannerException("'SQLEndTran' failed for SQL_ROLLBACK, return: " + std::to_string(ret) +
		                       ", diagnostics: '" + diag + "'");
	}

	Types::SetNullValueToResult(output, 0);
}

void OdbcRollbackFunction::Register(Extension &extension, Factory &factory) {
	auto fun = ScalarFunction::Create(factory);
	fun.SetName("odbc_rollback");
	FunctionSignature sig = fun.GetSignature();

	// parameters and return
	auto bigint_type = factory.CreateType(LogicalTypeId::BIGINT);
	auto varchar_type = factory.CreateType(LogicalTypeId::VARCHAR);
	sig.AddParameter("connection", bigint_type, FunctionParameterKind::POSITIONAL_ONLY);
	sig.SetReturnType(varchar_type);

	// callbacks
	fun.SetExecCallback(Rollback);

	// options
	fun.SetStability(duckdb::cxx::FunctionStability::VOLATILE);
	fun.SetNullHandling(duckdb::cxx::FunctionNullHandling::SPECIAL);

	// register and cleanup
	try {
		extension.Register(fun);
	} catch (std::exception &e) {
		throw ScannerException(std::string("\'") + "odbc_rollback" + "\' function registration failed: " + e.what());
	}
}

} // namespace odbcscanner
