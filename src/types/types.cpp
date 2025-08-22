#include "types.hpp"

#include "diagnostics.hpp"
#include "scanner_exception.hpp"
#include "widechar.hpp"

#include <vector>

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

OdbcType Types::GetResultColumnAttributes(const std::string &query, SQLSMALLINT cols_count, HSTMT hstmt,
                                          SQLUSMALLINT col_idx) {
	SQLLEN desc_type = -1;
	{
		SQLRETURN ret = SQLColAttributeW(hstmt, col_idx, SQL_DESC_TYPE, nullptr, 0, nullptr, &desc_type);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
			throw ScannerException(
			    "'SQLColAttribute' for SQL_DESC_TYPE failed, column index: " + std::to_string(col_idx) +
			    ", columns count: " + std::to_string(cols_count) + ", query: '" + query +
			    "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
		}
	}

	SQLLEN desc_concise_type = -1;
	{
		SQLRETURN ret =
		    SQLColAttributeW(hstmt, col_idx, SQL_DESC_CONCISE_TYPE, nullptr, 0, nullptr, &desc_concise_type);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
			throw ScannerException(
			    "'SQLColAttribute' for SQL_DESC_CONCISE_TYPE failed, column index: " + std::to_string(col_idx) +
			    ", columns count: " + std::to_string(cols_count) + ", query: '" + query +
			    "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
		}
	}

	std::vector<SQLWCHAR> buf;
	buf.resize(1024);
	SQLSMALLINT len_bytes = 0;
	{
		SQLRETURN ret = SQLColAttributeW(hstmt, col_idx, SQL_DESC_TYPE_NAME, buf.data(),
		                                 static_cast<SQLSMALLINT>(buf.size() * sizeof(SQLWCHAR)), &len_bytes, nullptr);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
			throw ScannerException(
			    "'SQLColAttribute' for SQL_DESC_TYPE_NAME failed, column index: " + std::to_string(col_idx) +
			    ", columns count: " + std::to_string(cols_count) + ", query: '" + query +
			    "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
		}
	}
	std::string desc_type_name = WideChar::Narrow(buf.data(), len_bytes * sizeof(SQLWCHAR));

	return OdbcType(desc_type, desc_concise_type, std::move(desc_type_name));
}

void Types::AddResultColumnOfType(duckdb_bind_info info, const std::string &name, const OdbcType &odbc_type) {
	switch (odbc_type.desc_concise_type) {
	case SQL_INTEGER: {
		TypeSpecific::AddResultColumn<int32_t>(info, name);
		return;
	}
	case SQL_VARCHAR: {
		TypeSpecific::AddResultColumn<std::string>(info, name);
		return;
	}
	default:
		throw ScannerException("Unsupported ODBC result type: " + std::to_string(odbc_type.desc_concise_type) +
		                       ", name: '" + odbc_type.desc_type_name + "'");
	}
}

ScannerParam Types::ExtractNotNullParamOfType(duckdb_type type_id, duckdb_vector vec, idx_t param_idx) {
	switch (type_id) {
	case DUCKDB_TYPE_INTEGER: {
		return TypeSpecific::ExtractNotNullParam<int32_t>(vec);
	}
	case DUCKDB_TYPE_VARCHAR: {
		return TypeSpecific::ExtractNotNullParam<std::string>(vec);
	}
	default:
		throw ScannerException("Cannot extract parameters from STRUCT: specified type is not supported, id: " +
		                       std::to_string(type_id) + ", index: " + std::to_string(param_idx));
	}
}

ScannerParam Types::ExtractNotNullParamOfType(duckdb_type type_id, duckdb_value value, idx_t param_idx) {
	switch (type_id) {
	case DUCKDB_TYPE_INTEGER: {
		return TypeSpecific::ExtractNotNullParam<int32_t>(value);
	}
	case DUCKDB_TYPE_VARCHAR: {
		return TypeSpecific::ExtractNotNullParam<std::string>(value);
	}
	default:
		throw ScannerException("Cannot extract parameters from STRUCT value: specified type is not supported, id: " +
		                       std::to_string(type_id) + ", index: " + std::to_string(param_idx));
	}
}

void Types::BindOdbcParam(const std::string &query, HSTMT hstmt, ScannerParam &param, SQLSMALLINT param_idx) {
	switch (param.TypeId()) {
	case DUCKDB_TYPE_SQLNULL: {
		TypeSpecific::BindOdbcParam<std::nullptr_t>(query, hstmt, param, param_idx);
		break;
	}
	case DUCKDB_TYPE_INTEGER: {
		TypeSpecific::BindOdbcParam<int32_t>(query, hstmt, param, param_idx);
		break;
	}
	case DUCKDB_TYPE_VARCHAR: {
		TypeSpecific::BindOdbcParam<std::string>(query, hstmt, param, param_idx);
		break;
	}
	default: {
		throw ScannerException("Unsupported parameter type: " + std::to_string(param.TypeId()));
	}
	}
}

void Types::FetchAndSetResultOfType(const OdbcType &odbc_type, const std::string &query, HSTMT hstmt,
                                    SQLSMALLINT col_idx, duckdb_vector vec, idx_t row_idx) {
	switch (odbc_type.desc_concise_type) {
	case SQL_INTEGER: {
		TypeSpecific::FetchAndSetResult<int32_t>(query, hstmt, col_idx, vec, row_idx);
		break;
	}
	case SQL_VARCHAR: {
		TypeSpecific::FetchAndSetResult<std::string>(query, hstmt, col_idx, vec, row_idx);
		break;
	}
	default:
		throw ScannerException("Unsupported ODBC fetch type: " + std::to_string(odbc_type.desc_concise_type) +
		                       ", name: '" + odbc_type.desc_type_name + "'");
	}
}

} // namespace odbcscanner
