#include "function_args.hpp"

#include "duckdb_v2.h"

namespace odbcscanner {

void FunctionArgs::AddNamedParameter(Factory &factory, FunctionSignature &sig, const std::string &name,
                                     const LogicalType &ltype) {
	// NULL value cannot be created with ANY type, VARCHAR NULL is used instead
	Value default_value = ltype.GetTypeId() == LogicalTypeId::ANY
	                          ? Value::CreateNull(factory, factory.CreateType(LogicalTypeId::VARCHAR))
	                          : Value::CreateNull(factory, ltype);
	sig.AddParameter(name, ltype, default_value, FunctionParameterKind::NAMED_ONLY);
}

std::unique_ptr<Value> FunctionArgs::GetArg(FunctionBindInput &input, idx_t index) {
	return std::unique_ptr<Value>(new Value(input.GetConstantArgument(index)));
}

std::unique_ptr<Value> FunctionArgs::GetNamedArg(FunctionBindInput &input, const std::string &name) {
	auto index = input.FindArg(name);
	if (!index.has_value()) {
		return std::unique_ptr<Value>();
	}
	auto val = GetArg(input, index.value());
	// Named parameters cannot be optional in C API V2, they have typed NULL default values
	// instead. Arguments for ANY parameters are not cast, so untyped NULL means that NULL
	// was specified explicitly. Explicit NULLs for typed parameters are cast to the
	// parameter type and are handled the same way as not specified parameters.
	if (val->IsNull() && val->GetLogicalType().GetTypeId() != LogicalTypeId::SQLNULL) {
		return std::unique_ptr<Value>();
	}
	return val;
}

duckdb::cxx::Exception FunctionArgs::BinderError(const std::string &message) {
	return duckdb::cxx::Exception(DUCKDB_V2_ERROR_QUERY_BINDER, message);
}

} // namespace odbcscanner
