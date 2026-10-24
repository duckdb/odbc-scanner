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

static void CancelQuery(ScalarFunction::ExecInput &input) {
	Vector output = input.GetResult();

	auto conn_arg = Types::ExtractFunctionArg<int64_t>(input, 0);
	if (conn_arg.second) {
		throw ScannerException("'odbc_cancel_query' error: specified ODBC connection must be not NULL");
	}
	int64_t conn_id = conn_arg.first;
	HSTMT stmt = CancellationRegistry::Get(conn_id);

	bool success = false;
	if (stmt != nullptr) {
		// the handle may or may not be valid
		SQLRETURN ret = SQLCancelHandle(SQL_HANDLE_STMT, stmt);
		if (SQL_SUCCEEDED(ret)) {
			success = true;
		}
	}

	bool *result_data = output.GetDataMutable<bool>();
	result_data[0] = success;
}

void OdbcCancelQueryFunction::Register(Extension &extension, Factory &factory) {
	auto fun = ScalarFunction::Create(factory);
	fun.SetName("odbc_cancel_query");
	FunctionSignature sig = fun.GetSignature();

	// parameters and return
	auto bigint_type = factory.CreateType(LogicalTypeId::BIGINT);
	auto boolean_type = factory.CreateType(LogicalTypeId::BOOLEAN);
	sig.AddParameter("connection", bigint_type, FunctionParameterKind::POSITIONAL_ONLY);
	sig.SetReturnType(boolean_type);

	// callbacks
	fun.SetExecCallback(CancelQuery);

	// options
	fun.SetStability(duckdb::cxx::FunctionStability::VOLATILE);
	fun.SetNullHandling(duckdb::cxx::FunctionNullHandling::SPECIAL);

	// register and cleanup
	try {
		extension.Register(fun);
	} catch (std::exception &e) {
		throw ScannerException(std::string("\'") + "odbc_cancel_query" +
		                       "\' function registration failed: " + e.what());
	}
}

} // namespace odbcscanner
