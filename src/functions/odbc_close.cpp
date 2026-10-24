#include "odbc_scanner.hpp"

#include <cstdint>
#include <string>

#include "function_args.hpp"
#include "connection.hpp"
#include "diagnostics.hpp"
#include "odbc_api.hpp"
#include "registries.hpp"
#include "scanner_exception.hpp"
#include "types.hpp"

namespace odbcscanner {

static void Close(ScalarFunction::ExecInput &input) {
	Vector output = input.GetResult();

	auto arg = Types::ExtractFunctionArg<int64_t>(input, 0);
	if (arg.second) {
		throw ScannerException("'odbc_close' error: specified ODBC connection argument must be not NULL");
	}
	{
		auto conn_ptr = ConnectionsRegistry::Remove(arg.first);
		// connection may or may not be already closed - we won't throw from this function either way
	}

	Types::SetNullValueToResult(output, 0);
}

void OdbcCloseFunction::Register(Extension &extension, Factory &factory) {
	auto fun = ScalarFunction::Create(factory);
	fun.SetName("odbc_close");
	FunctionSignature sig = fun.GetSignature();

	// parameters and return
	auto bigint_type = factory.CreateType(LogicalTypeId::BIGINT);
	auto varchar_type = factory.CreateType(LogicalTypeId::VARCHAR);
	sig.AddParameter("connection", bigint_type, FunctionParameterKind::POSITIONAL_ONLY);
	sig.SetReturnType(varchar_type);

	// callbacks
	fun.SetExecCallback(Close);

	// options
	fun.SetNullHandling(duckdb::cxx::FunctionNullHandling::SPECIAL);
	fun.SetStability(duckdb::cxx::FunctionStability::VOLATILE);

	// register and cleanup
	try {
		extension.Register(fun);
	} catch (std::exception &e) {
		throw ScannerException(std::string("\'") + "odbc_close" + "\' function registration failed: " + e.what());
	}
}

} // namespace odbcscanner
