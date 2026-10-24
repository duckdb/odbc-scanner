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

static void BeginTransaction(ScalarFunction::ExecInput &input) {
	Vector output = input.GetResult();

	auto conn_arg = Types::ExtractFunctionArg<int64_t>(input, 0);
	if (conn_arg.second) {
		throw ScannerException("'odbc_begin_transaction' error: specified ODBC connection must be not NULL");
	}
	int64_t conn_id = conn_arg.first;
	auto conn_ptr = ConnectionsRegistry::Remove(conn_id);

	if (conn_ptr.get() == nullptr) {
		throw ScannerException("'odbc_begin_transaction' error: open ODBC connection not found, id: " +
		                       std::to_string(conn_id));
	}

	// Return the connection to registry at the end of the block
	auto deferred_conn = Defer([&conn_ptr] { ConnectionsRegistry::Add(std::move(conn_ptr)); });

	OdbcConnection &conn = *conn_ptr;

	SQLRETURN ret = SQLSetConnectAttr(conn.dbc, SQL_ATTR_AUTOCOMMIT, static_cast<SQLPOINTER>(SQL_AUTOCOMMIT_OFF), 0);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(conn.dbc, SQL_HANDLE_DBC);
		throw ScannerException("'SQLSetConnectAttr' failed for SQL_AUTOCOMMIT_OFF, return: " + std::to_string(ret) +
		                       ", diagnostics: '" + diag + "'");
	}

	Types::SetNullValueToResult(output, 0);
}

void OdbcBeginTransactionFunction::Register(Extension &extension, Factory &factory) {
	auto fun = ScalarFunction::Create(factory);
	fun.SetName("odbc_begin_transaction");
	FunctionSignature sig = fun.GetSignature();

	// parameters and return
	auto bigint_type = factory.CreateType(LogicalTypeId::BIGINT);
	auto varchar_type = factory.CreateType(LogicalTypeId::VARCHAR);
	sig.AddParameter("connection", bigint_type, FunctionParameterKind::POSITIONAL_ONLY);
	sig.SetReturnType(varchar_type);

	// callbacks
	fun.SetExecCallback(BeginTransaction);

	// options
	fun.SetStability(duckdb::cxx::FunctionStability::VOLATILE);
	fun.SetNullHandling(duckdb::cxx::FunctionNullHandling::SPECIAL);

	// register and cleanup
	try {
		extension.Register(fun);
	} catch (std::exception &e) {
		throw ScannerException(std::string("\'") + "odbc_begin_transaction" +
		                       "\' function registration failed: " + e.what());
	}
}

} // namespace odbcscanner
