#pragma once

#include <memory>
#include <string>

#include "duckdb_extension_api.hpp"

namespace odbcscanner {

// Helpers for function arguments handling that are not covered by C++ API
struct FunctionArgs {

	// Adds named parameter that defaults to NULL when not specified,
	// named parameters without default values are mandatory
	static void AddNamedParameter(Factory &factory, FunctionSignature &sig, const std::string &name,
	                              const LogicalType &ltype);

	// Returns constant value of the argument
	static std::unique_ptr<Value> GetArg(FunctionBindInput &input, idx_t index);

	// Returns NULL pointer if named argument was not specified, for typed (not ANY) parameters
	// returns NULL pointer also when argument was specified as NULL
	static std::unique_ptr<Value> GetNamedArg(FunctionBindInput &input, const std::string &name);

	// Exception to report from bind callbacks
	static duckdb::cxx::Exception BinderError(const std::string &message);
};

} // namespace odbcscanner
