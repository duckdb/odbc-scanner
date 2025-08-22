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
		Types::AddResultColumn<int32_t>(info, name);
		return;
	}
	case SQL_VARCHAR: {
		Types::AddResultColumn<std::string>(info, name);
		return;
	}
	default:
		throw ScannerException("Unsupported ODBC result type: " + std::to_string(odbc_type.desc_concise_type) +
		                       ", name: '" + odbc_type.desc_type_name + "'");
	}
}

} // namespace odbcscanner
