#include "params.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "function_args.hpp"
#include "connection.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"
#include "types.hpp"

namespace odbcscanner {

std::vector<ScannerValue> Params::Extract(const DbmsQuirks &quirks, const Value *struct_value) {
	if (struct_value->IsNull()) {
		throw ScannerException("Cannot extract parameters from STRUCT: specified STRUCT is NULL");
	}
	LogicalTypeId struct_type_id = Types::TypeIdOf(struct_value->GetLogicalType());
	if (struct_type_id != LogicalTypeId::STRUCT) {
		throw ScannerException("Cannot extract parameters from STRUCT: specified value is not STRUCT");
	}
	idx_t params_count = struct_value->GetChildCount();

	std::vector<ScannerValue> params;
	for (idx_t i = 0; i < params_count; i++) {
		auto child_val = std::unique_ptr<Value>(new Value(struct_value->GetChild(i)));
		if (child_val->IsNull()) {
			params.emplace_back(ScannerValue());
			continue;
		}

		ScannerValue sp = Types::ExtractNotNullParam(quirks, child_val.get(), i);
		params.emplace_back(std::move(sp));
	}

	return params;
}

std::vector<SQLSMALLINT> Params::CollectTypes(const QueryContext &ctx) {
	SQLSMALLINT count = -1;
	{
		SQLRETURN ret = SQLNumParams(ctx.hstmt(), &count);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
			throw ScannerException("'SQLNumParams' failed, query: '" + ctx.query + "', return: " + std::to_string(ret) +
			                       ", diagnostics: '" + diag + "'");
		}
	}

	std::vector<SQLSMALLINT> param_types;
	for (SQLSMALLINT i = 0; i < count; i++) {
		SQLSMALLINT param_idx = i + 1;

		SQLSMALLINT ptype = -1;
		SQLULEN size_out = 0;
		SQLSMALLINT dec_digits_out = 0;
		SQLSMALLINT nullable_out = 0;
		SQLRETURN ret = SQLDescribeParam(ctx.hstmt(), param_idx, &ptype, &size_out, &dec_digits_out, &nullable_out);
		if (!SQL_SUCCEEDED(ret)) { // SQLDescribeParam may or may not be supported
			ptype = SQL_TYPE_NULL;
		}

		param_types.push_back(ptype);
	}

	return param_types;
}

// When the prepared-parameter's target is a character SQL type and the source is
// numeric, stringify in the scanner. This short-circuits the driver's
// numeric-C → character-SQL conversion path, which has produced silent data loss
// in multiple ODBC drivers (FirebirdSQL/firebird-odbc-driver#292 and older
// MSSQL/MySQL releases). Done here — before binding — so that post-SetExpectedTypes
// the param's type_id is final. Wide targets (SQL_WCHAR/WVARCHAR/WLONGVARCHAR)
// are handled by BindOdbcParam<DecimalChars>, which widens the buffer on demand.
static void CoalesceNumericToCharsIfNeeded(ScannerValue &param) {
	if (!Types::IsCharacterSQLType(param.ExpectedType())) {
		return;
	}
	switch (param.ParamType()) {
	case ToParamType(LogicalTypeId::TINYINT):
	case ToParamType(LogicalTypeId::UTINYINT):
	case ToParamType(LogicalTypeId::SMALLINT):
	case ToParamType(LogicalTypeId::USMALLINT):
	case ToParamType(LogicalTypeId::INTEGER):
	case ToParamType(LogicalTypeId::UINTEGER):
	case ToParamType(LogicalTypeId::BIGINT):
	case ToParamType(LogicalTypeId::UBIGINT):
	case ToParamType(LogicalTypeId::FLOAT):
	case ToParamType(LogicalTypeId::DOUBLE):
		param.TransformNumericToChars();
		break;
	default:
		break;
	}
}

void Params::SetExpectedTypes(const QueryContext &ctx, const std::vector<SQLSMALLINT> &expected,
                              std::vector<ScannerValue> &actual) {
	if (expected.size() != actual.size()) {
		throw ScannerException("Incorrect number of parameters specified, query: '" + ctx.query + "', expected: " +
		                       std::to_string(expected.size()) + ", actual: " + std::to_string(actual.size()));
	}
	for (size_t i = 0; i < actual.size(); i++) {
		SQLSMALLINT expected_type = expected.at(i);
		ScannerValue &param = actual.at(i);
		Types::CoalesceParameterType(ctx, param);
		param.SetExpectedType(expected_type);
		CoalesceNumericToCharsIfNeeded(param);
	}
}

void Params::BindToOdbc(const QueryContext &ctx, std::vector<ScannerValue> &params) {
	if (params.size() == 0) {
		return;
	}

	SQLRETURN ret = SQLFreeStmt(ctx.hstmt(), SQL_RESET_PARAMS);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
		throw ScannerException("'SQLFreeStmt' SQL_RESET_PARAMS failed, diagnostics: '" + diag + "'");
	}

	for (size_t i = 0; i < params.size(); i++) {
		ScannerValue &param = params.at(i);
		SQLSMALLINT idx = static_cast<SQLSMALLINT>(i + 1);
		Types::BindOdbcParam(ctx, param, idx);
	}
}

// Slots whose backing buffer lives in a dynamically sized container (std::vector
// inside DecimalChars / WideString / ScannerBlob / ScannerUuid) can move as the
// value is reassigned — the previously bound pointer would then dangle. Only
// fixed-width slots are safe to reuse across executes without rebinding.
static bool IsFixedWidthShape(param_type type_id) {
	switch (type_id) {
	case ToParamType(LogicalTypeId::SQLNULL):
	case ToParamType(LogicalTypeId::BOOLEAN):
	case ToParamType(LogicalTypeId::TINYINT):
	case ToParamType(LogicalTypeId::UTINYINT):
	case ToParamType(LogicalTypeId::SMALLINT):
	case ToParamType(LogicalTypeId::USMALLINT):
	case ToParamType(LogicalTypeId::INTEGER):
	case ToParamType(LogicalTypeId::UINTEGER):
	case ToParamType(LogicalTypeId::BIGINT):
	case ToParamType(LogicalTypeId::UBIGINT):
	case ToParamType(LogicalTypeId::FLOAT):
	case ToParamType(LogicalTypeId::DOUBLE):
	case ToParamType(LogicalTypeId::DECIMAL):
	case ToParamType(LogicalTypeId::DATE):
	case ToParamType(LogicalTypeId::TIME):
	case ToParamType(LogicalTypeId::TIMESTAMP):
	case ToParamType(LogicalTypeId::TIMESTAMP_TZ):
	case Params::TYPE_TIME_WITH_NANOS:
	case Params::TYPE_SQL_BIT:
	case Params::TYPE_SQL_GUID:
	case Params::TYPE_SS_TIMESTAMPOFFSET:
		return true;
	default:
		return false;
	}
}

void Params::BindToOdbcWithCache(const QueryContext &ctx, std::vector<ScannerValue> &params, BindCache &cache) {
	if (params.empty()) {
		return;
	}

	// Size mismatch (first call after Reset, or column-count change) — clear the
	// cache so every slot gets bound fresh, and call SQL_RESET_PARAMS first to
	// drop any leftover bindings from a previous prepared statement on the same
	// hstmt. SQLPrepare does NOT release parameter bindings — only
	// SQL_RESET_PARAMS (or rebinding the same index) does — so without this a
	// 16-row batch statement transitioning to a 1-row tail statement leaves 15
	// stale bindings pointing at addresses the new statement has no parameter
	// indices for, which segfaults at least the DuckDB ODBC driver.
	if (cache.shape.size() != params.size()) {
		SQLRETURN ret = SQLFreeStmt(ctx.hstmt(), SQL_RESET_PARAMS);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
			throw ScannerException("'SQLFreeStmt' SQL_RESET_PARAMS failed, diagnostics: '" + diag + "'");
		}
		cache.shape.assign(params.size(), BindSlotShape());
	}

	for (size_t i = 0; i < params.size(); i++) {
		ScannerValue &p = params.at(i);
		BindSlotShape current(p.ParamType(), p.ExpectedType());
		// Rebind when the slot's logical shape changed, OR when the slot is
		// variable-width (its buffer pointer may have moved even with the same
		// shape). Fixed-width + same-shape slots are intentionally left bound to
		// their previous address — the value at that address has been updated
		// in-place by the caller and the next SQLExecute will pick it up.
		bool needs_rebind = (cache.shape.at(i) != current) || !IsFixedWidthShape(current.type_id);
		if (!needs_rebind) {
			continue;
		}
		SQLSMALLINT idx = static_cast<SQLSMALLINT>(i + 1);
		Types::BindOdbcParam(ctx, p, idx);
		cache.shape.at(i) = current;
	}
}

} // namespace odbcscanner
