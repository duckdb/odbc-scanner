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

	SQLLEN is_unsigned = -1;
	{
		SQLRETURN ret = SQLColAttributeW(hstmt, col_idx, SQL_DESC_UNSIGNED, nullptr, 0, nullptr, &is_unsigned);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
			throw ScannerException(
			    "'SQLColAttribute' for SQL_DESC_UNSIGNED failed, column index: " + std::to_string(col_idx) +
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
	std::string desc_type_name = WideChar::Narrow(buf.data(), len_bytes / sizeof(SQLWCHAR));

	uint8_t decimal_precision = 0;
	uint8_t decimal_scale = 0;
	if (desc_concise_type == SQL_DECIMAL || desc_concise_type == SQL_NUMERIC) {
		{
			SQLLEN precision = -1;
			SQLRETURN ret = SQLColAttributeW(hstmt, col_idx, SQL_DESC_PRECISION, nullptr, 0, nullptr, &precision);
			if (!SQL_SUCCEEDED(ret)) {
				std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
				throw ScannerException(
				    "'SQLColAttribute' for SQL_DESC_PRECISION failed, column index: " + std::to_string(col_idx) +
				    ", columns count: " + std::to_string(cols_count) + ", query: '" + query +
				    "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
			}
			decimal_precision = static_cast<uint8_t>(precision);
		}

		{
			SQLLEN scale = -1;
			SQLRETURN ret = SQLColAttributeW(hstmt, col_idx, SQL_DESC_SCALE, nullptr, 0, nullptr, &scale);
			if (!SQL_SUCCEEDED(ret)) {
				std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
				throw ScannerException(
				    "'SQLColAttribute' for SQL_DESC_SCALE failed, column index: " + std::to_string(col_idx) +
				    ", columns count: " + std::to_string(cols_count) + ", query: '" + query +
				    "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
			}
			decimal_scale = static_cast<uint8_t>(scale);
		}
	}

	return OdbcType(desc_type, desc_concise_type, is_unsigned == SQL_TRUE, std::move(desc_type_name), decimal_precision,
	                decimal_scale);
}

std::vector<ResultColumn> Columns::Collect(QueryContext &ctx) {

	SQLSMALLINT cols_count = -1;
	{
		SQLRETURN ret = SQLNumResultCols(ctx.hstmt, &cols_count);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
			throw ScannerException("'SQLNumResultCols' failed, query: '" + ctx.query +
			                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
		}
	}

	std::vector<ResultColumn> vec;

	for (SQLSMALLINT col_idx = 1; col_idx <= cols_count; col_idx++) {
		std::vector<SQLWCHAR> buf;
		buf.resize(1024);
		SQLSMALLINT len_bytes = 0;
		{
			SQLRETURN ret =
			    SQLColAttributeW(ctx.hstmt, col_idx, SQL_DESC_NAME, buf.data(),
			                     static_cast<SQLSMALLINT>(buf.size() * sizeof(SQLWCHAR)), &len_bytes, nullptr);
			if (!SQL_SUCCEEDED(ret)) {
				std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
				throw ScannerException(
				    "'SQLColAttribute' for SQL_DESC_NAME failed, column index: " + std::to_string(col_idx) +
				    ", columns count: " + std::to_string(cols_count) + ", query: '" + ctx.query +
				    "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
			}
		}
		std::string name = WideChar::Narrow(buf.data(), len_bytes / sizeof(SQLWCHAR));
		OdbcType odbc_type = GetTypeAttributes(ctx.query, cols_count, ctx.hstmt, col_idx);
		ResultColumn col(std::move(name), std::move(odbc_type));
		vec.emplace_back(std::move(col));
	}

	return vec;
}

void Columns::CheckSame(QueryContext &ctx, std::vector<ResultColumn> &expected, std::vector<ResultColumn> &actual) {
	if (expected.size() != actual.size()) {
		throw ScannerException("Resulting columns from 'SQLPrepare' and 'SQLExecute' do not match, query: '" +
		                       ctx.query + "',  expected count: " + std::to_string(expected.size()) +
		                       ", actual count: " + std::to_string(actual.size()));
	}
	for (size_t i = 0; i < expected.size(); i++) {
		auto &expected_col = expected.at(i);
		auto &actual_col = actual.at(i);

		if (expected_col.name != actual_col.name) {
			throw ScannerException("Resulting columns from 'SQLPrepare' and 'SQLExecute' do not match, query: '" +
			                       ctx.query + "', index: " + std::to_string(i) + ", expected name: '" +
			                       expected_col.name + "', actual name: '" + actual_col.name + "'");
		}

		// In some drivers the `PREPARE` and `EXECUTE` column types may be different,
		// lets rely on the driver to do the conversion instead of erroring out.
		/*
		if (!expected_col.odbc_type.Equals(actual_col.odbc_type)) {
		    throw ScannerException("Resulting columns from 'SQLPrepare' and 'SQLExecute' do not match, query: '" +
		                           ctx.query + "',  index: " + std::to_string(i) + ", expected: '" +
		                           expected_col.odbc_type.ToString() + "', actual: '" +
		                           actual_col.odbc_type.ToString() + "'");
		}
		*/
	}
}

void Columns::AddToResults(duckdb_bind_info info, duckdb_type type_id, ResultColumn &col) {
	if (type_id == DUCKDB_TYPE_DECIMAL) {
		auto ltype =
		    LogicalTypePtr(duckdb_create_decimal_type(col.odbc_type.decimal_precision, col.odbc_type.decimal_scale),
		                   LogicalTypeDeleter);
		duckdb_bind_add_result_column(info, col.name.c_str(), ltype.get());
	} else {
		auto ltype = LogicalTypePtr(duckdb_create_logical_type(type_id), LogicalTypeDeleter);
		duckdb_bind_add_result_column(info, col.name.c_str(), ltype.get());
	}
}

} // namespace odbcscanner