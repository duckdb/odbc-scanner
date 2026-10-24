#include "types.hpp"

#include "diagnostics.hpp"
#include "scanner_exception.hpp"

namespace odbcscanner {

template <>
void TypeSpecific::BindOdbcParam<std::nullptr_t>(const QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLRETURN ret = SQLBindParameter(ctx.hstmt(), param_idx, SQL_PARAM_INPUT, SQL_C_DEFAULT, param.ExpectedType(), 1, 0,
	                                 nullptr, 0, &param.LengthBytes());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
		throw ScannerException(
		    "'SQLBindParameter' NULL failed, expected type: " + std::to_string(param.ExpectedType()) +
		    " index: " + std::to_string(param_idx) + ", query: '" + ctx.query + "', return: " + std::to_string(ret) +
		    ", diagnostics: '" + diag + "'");
	}
}

void Types::SetNullValueToResult(Vector &vec, idx_t row_idx) {
	if (vec.GetVectorType() == duckdb::cxx::VectorType::CONSTANT) {
		vec.SetConstantValid(false);
		return;
	}
	vec.GetValidityMutable().SetInvalid(row_idx);
}

LogicalTypeId Types::TypeIdOf(const LogicalType &ltype) {
	LogicalTypeId type_id = ltype.GetTypeId();
	if (type_id == LogicalTypeId::TUPLE) {
		return LogicalTypeId::STRUCT;
	}
	return type_id;
}

} // namespace odbcscanner
