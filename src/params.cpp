#include "params.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "capi_pointers.hpp"
#include "connection.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"
#include "types.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

std::vector<ScannerValue> Params::Extract(DbmsQuirks &quirks, duckdb_data_chunk chunk, idx_t col_idx) {
	(void)col_idx;
	idx_t col_count = duckdb_data_chunk_get_column_count(chunk);
	if (col_idx >= col_count) {
		throw ScannerException("Cannot extract parameters from STRUCT: column not found, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	duckdb_vector vec = duckdb_data_chunk_get_vector(chunk, col_idx);
	if (vec == nullptr) {
		throw ScannerException("Cannot extract parameters from STRUCT: vector is NULL, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	idx_t rows_count = duckdb_data_chunk_get_size(chunk);
	if (rows_count == 0) {
		throw ScannerException("Cannot extract parameters from STRUCT: vector contains no rows, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	uint64_t *validity = duckdb_vector_get_validity(vec);
	if (validity != nullptr && !duckdb_validity_row_is_valid(validity, 0)) {
		throw ScannerException("Cannot extract parameters from STRUCT: specified STRUCT is NULL, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	auto struct_type = LogicalTypePtr(duckdb_vector_get_column_type(vec), LogicalTypeDeleter);
	duckdb_type struct_type_id = duckdb_get_type_id(struct_type.get());
	if (struct_type_id != DUCKDB_TYPE_STRUCT) {
		throw ScannerException(
		    "Cannot extract parameters from STRUCT: specified value is not STRUCT, column: " + std::to_string(col_idx) +
		    ", columns count: " + std::to_string(col_count) + ", type ID: " + std::to_string(struct_type_id));
	}

	idx_t params_count = duckdb_struct_type_child_count(struct_type.get());
	if (params_count == 0) {
		throw ScannerException("Cannot extract parameters from STRUCT: specified STRUCT has no fields, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	std::vector<ScannerValue> params;
	for (idx_t i = 0; i < params_count; i++) {
		auto child_vec = duckdb_struct_vector_get_child(vec, i);
		if (child_vec == nullptr) {
			throw ScannerException(
			    "Cannot extract parameters from STRUCT: child vector is NULL, index: " + std::to_string(i) +
			    ", column: " + std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
		}

		uint64_t *child_validity = duckdb_vector_get_validity(child_vec);
		if (child_validity != nullptr && !duckdb_validity_row_is_valid(child_validity, 0)) {
			params.emplace_back(ScannerValue());
			continue;
		}

		auto child_type = LogicalTypePtr(duckdb_struct_type_child_type(struct_type.get(), i), LogicalTypeDeleter);
		auto child_type_id = duckdb_get_type_id(child_type.get());
		ScannerValue sp = Types::ExtractNotNullParam(quirks, child_type_id, child_vec, 0, i);
		params.emplace_back(std::move(sp));
	}

	return params;
}

std::vector<ScannerValue> Params::Extract(DbmsQuirks &quirks, duckdb_value struct_value) {
	if (duckdb_is_null_value(struct_value)) {
		throw ScannerException("Cannot extract parameters from STRUCT: specified STRUCT is NULL");
	}
	duckdb_logical_type struct_type = duckdb_get_value_type(struct_value);
	duckdb_type struct_type_id = duckdb_get_type_id(struct_type);
	if (struct_type_id != DUCKDB_TYPE_STRUCT) {
		throw ScannerException("Cannot extract parameters from STRUCT: specified value is not STRUCT");
	}
	idx_t params_count = duckdb_struct_type_child_count(struct_type);

	std::vector<ScannerValue> params;
	for (idx_t i = 0; i < params_count; i++) {
		auto child_val = ValuePtr(duckdb_get_struct_child(struct_value, i), ValueDeleter);
		if (duckdb_is_null_value(child_val.get())) {
			params.emplace_back(ScannerValue());
			continue;
		}

		ScannerValue sp = Types::ExtractNotNullParam(quirks, child_val.get(), i);
		params.emplace_back(std::move(sp));
	}

	return params;
}

std::vector<SQLSMALLINT> Params::CollectTypes(QueryContext &ctx) {
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
	case DUCKDB_TYPE_TINYINT:
	case DUCKDB_TYPE_UTINYINT:
	case DUCKDB_TYPE_SMALLINT:
	case DUCKDB_TYPE_USMALLINT:
	case DUCKDB_TYPE_INTEGER:
	case DUCKDB_TYPE_UINTEGER:
	case DUCKDB_TYPE_BIGINT:
	case DUCKDB_TYPE_UBIGINT:
	case DUCKDB_TYPE_FLOAT:
	case DUCKDB_TYPE_DOUBLE:
		param.TransformNumericToChars();
		break;
	default:
		break;
	}
}

void Params::SetExpectedTypes(QueryContext &ctx, const std::vector<SQLSMALLINT> &expected,
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

void Params::BindToOdbc(QueryContext &ctx, std::vector<ScannerValue> &params) {
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
// inside DecimalChars / WideString / ScannerBlob, etc.) can reallocate as the
// value changes; the previously-bound pointer would then be invalid. Only
// fixed-width slots are safe to reuse across executes without rebinding.
static bool IsFixedWidthShape(param_type type_id) {
	switch (type_id) {
	case DUCKDB_TYPE_SQLNULL:
	case DUCKDB_TYPE_BOOLEAN:
	case DUCKDB_TYPE_TINYINT:
	case DUCKDB_TYPE_UTINYINT:
	case DUCKDB_TYPE_SMALLINT:
	case DUCKDB_TYPE_USMALLINT:
	case DUCKDB_TYPE_INTEGER:
	case DUCKDB_TYPE_UINTEGER:
	case DUCKDB_TYPE_BIGINT:
	case DUCKDB_TYPE_UBIGINT:
	case DUCKDB_TYPE_FLOAT:
	case DUCKDB_TYPE_DOUBLE:
	case DUCKDB_TYPE_DECIMAL:
	case DUCKDB_TYPE_DATE:
	case DUCKDB_TYPE_TIME:
	case DUCKDB_TYPE_TIMESTAMP:
	case DUCKDB_TYPE_TIMESTAMP_TZ:
	case Params::TYPE_TIME_WITH_NANOS:
	case Params::TYPE_SQL_BIT:
	case Params::TYPE_SQL_GUID:
	case Params::TYPE_SS_TIMESTAMPOFFSET:
		return true;
	default:
		return false;
	}
}

bool Params::BindToOdbcIfShapeUnchanged(QueryContext &ctx, std::vector<ScannerValue> &params, BindCache &cache) {
	if (params.size() == 0) {
		return false;
	}

	std::vector<BindSlotShape> shape;
	shape.reserve(params.size());
	bool all_fixed_width = true;
	for (size_t i = 0; i < params.size(); i++) {
		ScannerValue &p = params.at(i);
		BindSlotShape s;
		s.type_id = p.ParamType();
		s.expected_type = p.ExpectedType();
		shape.push_back(s);
		if (!IsFixedWidthShape(s.type_id)) {
			all_fixed_width = false;
		}
	}

	bool can_reuse = cache.initialized && all_fixed_width && cache.shape.size() == shape.size();
	if (can_reuse) {
		for (size_t i = 0; i < shape.size(); i++) {
			if (cache.shape.at(i) != shape.at(i)) {
				can_reuse = false;
				break;
			}
		}
	}

	if (can_reuse) {
		return true;
	}

	BindToOdbc(ctx, params);
	cache.shape = std::move(shape);
	cache.initialized = true;
	return false;
}

} // namespace odbcscanner
