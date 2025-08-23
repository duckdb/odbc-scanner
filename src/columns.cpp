#include "columns.hpp"

#include <cstdint>
#include <vector>

#include "diagnostics.hpp"
#include "scanner_exception.hpp"
#include "widechar.hpp"

namespace odbcscanner {

static OdbcType GetTypeAttributes(const std::string &query, SQLSMALLINT cols_count, HSTMT hstmt, SQLUSMALLINT col_idx) {

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

std::vector<ResultColumn> Columns::Collect(const std::string &query, HSTMT hstmt) {

	SQLSMALLINT cols_count = -1;
	{
		SQLRETURN ret = SQLNumResultCols(hstmt, &cols_count);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
			throw ScannerException("'SQLNumResultCols' failed, query: '" + query + "', return: " + std::to_string(ret) +
			                       ", diagnostics: '" + diag + "'");
		}
	}

	std::vector<ResultColumn> vec;

	for (SQLSMALLINT col_idx = 1; col_idx <= cols_count; col_idx++) {
		std::vector<SQLWCHAR> buf;
		buf.resize(1024);
		SQLSMALLINT len_bytes = 0;
		{
			SQLRETURN ret =
			    SQLColAttributeW(hstmt, col_idx, SQL_DESC_NAME, buf.data(),
			                     static_cast<SQLSMALLINT>(buf.size() * sizeof(SQLWCHAR)), &len_bytes, nullptr);
			if (!SQL_SUCCEEDED(ret)) {
				std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
				throw ScannerException(
				    "'SQLColAttribute' for SQL_DESC_NAME failed, column index: " + std::to_string(col_idx) +
				    ", columns count: " + std::to_string(cols_count) + ", query: '" + query +
				    "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
			}
		}
		std::string name = WideChar::Narrow(buf.data(), len_bytes * sizeof(SQLWCHAR));
		OdbcType odbc_type = GetTypeAttributes(query, cols_count, hstmt, col_idx);
		ResultColumn col(std::move(name), std::move(odbc_type));
		vec.emplace_back(std::move(col));
	}

	return vec;
}

void Columns::CheckSame(const std::string &query, std::vector<ResultColumn> &expected,
                        std::vector<ResultColumn> &actual) {
	if (expected.size() != actual.size()) {
		throw ScannerException("Resulting columns from 'SQLPrepare' and 'SQLExecute' do not match, query: '" + query +
		                       "',  expected count: " + std::to_string(expected.size()) +
		                       ", actual count: " + std::to_string(actual.size()));
	}
	for (size_t i = 0; i < expected.size(); i++) {
		auto &expected_col = expected.at(i);
		auto &actual_col = actual.at(i);

		if (expected_col.name != actual_col.name) {
			throw ScannerException("Resulting columns from 'SQLPrepare' and 'SQLExecute' do not match, query: '" +
			                       query + "', index: " + std::to_string(i) + ", expected name: '" + expected_col.name +
			                       "', actual name: '" + actual_col.name + "'");
		}

		if (!expected_col.odbc_type.Equals(actual_col.odbc_type)) {
			throw ScannerException("Resulting columns from 'SQLPrepare' and 'SQLExecute' do not match, query: '" +
			                       query + "',  index: " + std::to_string(i) + ", expected: '" +
			                       expected_col.odbc_type.ToString() + "', actual: '" +
			                       actual_col.odbc_type.ToString() + "'");
		}
	}
}

} // namespace odbcscanner