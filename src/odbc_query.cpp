#include "capi_odbc_scanner.h"
#include "capi_pointers.hpp"
#include "common.hpp"
#include "fetch.hpp"
#include "odbc_connection.hpp"
#include "params.hpp"
#include "scanner_exception.hpp"
#include "types.hpp"
#include "widechar.hpp"

#include <memory>
#include <sql.h>
#include <sqlext.h>
#include <string>
#include <vector>

DUCKDB_EXTENSION_EXTERN

static void odbc_query_bind(duckdb_bind_info info) noexcept;
static void odbc_query_init(duckdb_init_info info) noexcept;
static void odbc_query_local_init(duckdb_init_info info) noexcept;
static void odbc_query_function(duckdb_function_info info, duckdb_data_chunk output) noexcept;

namespace odbcscanner {

struct BindData {
	OdbcConnection &conn;
	std::string query;
	std::vector<ValuePtr> params;
	std::vector<void *> params_holder;
	HSTMT hstmt = SQL_NULL_HSTMT;

	BindData(OdbcConnection &conn_in, std::string query_in, std::vector<ValuePtr> params_in, HSTMT hstmt_in)
	    : conn(conn_in), query(std::move(query_in)), params(std::move(params_in)), hstmt(hstmt_in) {
	}

	BindData &operator=(const BindData &) = delete;
	BindData &operator=(BindData &&other) = delete;

	~BindData() noexcept {
		SQLFreeStmt(hstmt, SQL_CLOSE);
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
		for (void *param : params_holder) {
			std::free(param);
		}
	}

	static void Destroy(void *bdata_in) noexcept {
		auto bdata = reinterpret_cast<BindData *>(bdata_in);
		delete bdata;
	}
};

struct LocalInitData {
	OdbcConnection &conn;

	// todo: enum
	bool finished = false;

	LocalInitData(OdbcConnection &conn_in) : conn(conn_in) {
	}

	LocalInitData &operator=(const LocalInitData &) = delete;
	LocalInitData &operator=(LocalInitData &&other);

	static void Destroy(void *ctx_in) noexcept {
		auto ctx = reinterpret_cast<LocalInitData *>(ctx_in);
		delete ctx;
	}
};

static void Bind(duckdb_bind_info info) {
	auto conn_ptr_val = ValuePtr(duckdb_bind_get_parameter(info, 0), ValueDeleter);
	if (duckdb_is_null_value(conn_ptr_val.get())) {
		throw ScannerException("'odbc_query': specified ODBC connection is NULL");
	}
	int64_t conn_ptr_num = duckdb_get_int64(conn_ptr_val.get());
	OdbcConnection &conn = *reinterpret_cast<OdbcConnection *>(conn_ptr_num);

	auto query_val = ValuePtr(duckdb_bind_get_parameter(info, 1), ValueDeleter);
	if (duckdb_is_null_value(query_val.get())) {
		throw ScannerException("'odbc_query': specified SQL query is NULL");
	}
	auto query_ptr = VarcharPtr(duckdb_get_varchar(query_val.get()), VarcharDeleter);
	std::string query(query_ptr.get());

	auto params_ptr_val = ValuePtr(duckdb_bind_get_named_parameter(info, "params"), ValueDeleter);
	std::vector<ValuePtr> params;
	if (params_ptr_val.get() != nullptr && !duckdb_is_null_value(params_ptr_val.get())) {
		int64_t params_ptr_num = duckdb_get_int64(params_ptr_val.get());
		std::vector<ValuePtr> *passed_params_ptr = reinterpret_cast<std::vector<ValuePtr> *>(params_ptr_num);
		std::vector<ValuePtr> &passed_params = *passed_params_ptr;
		for (ValuePtr &vp : passed_params) {
			params.push_back(std::move(vp));
		}
		delete passed_params_ptr;
	}

	HSTMT hstmt = SQL_NULL_HSTMT;
	{
		SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, conn.dbc, &hstmt);
		if (!SQL_SUCCEEDED(ret)) {
			throw ScannerException("'SQLAllocHandle' failed for STMT handle, return: " + std::to_string(ret));
		}
	}
	{
		auto wquery = utf8_to_utf16_lenient(query.data(), query.length());
		SQLRETURN ret = SQLPrepareW(hstmt, wquery.data(), wquery.length<SQLINTEGER>());
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = ReadDiagnostics(hstmt, SQL_HANDLE_STMT);
			throw ScannerException("'SQLPrepare' failed, query: '" + query + "', return: " + std::to_string(ret) +
			                       ", diagnostics: '" + diag + "'");
		}
	}

	{
		SQLSMALLINT count = -1;
		SQLRETURN ret = SQLNumParams(hstmt, &count);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = ReadDiagnostics(hstmt, SQL_HANDLE_STMT);
			throw ScannerException("'SQLNumParams' failed, query: '" + query + "', return: " + std::to_string(ret) +
			                       ", diagnostics: '" + diag + "'");
		}
		if (static_cast<size_t>(count) != params.size()) {
			throw ScannerException(
			    "Incorrect number of query parameters specified, expected: " + std::to_string(count) +
			    ", actual: " + std::to_string(params.size()) + ", query: '" + query + "'");
		}
	}

	SQLSMALLINT cols_count = -1;
	{
		SQLRETURN ret = SQLNumResultCols(hstmt, &cols_count);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = ReadDiagnostics(hstmt, SQL_HANDLE_STMT);
			throw ScannerException("'SQLNumResultCols' failed, query: '" + query + "', return: " + std::to_string(ret) +
			                       ", diagnostics: '" + diag + "'");
		}
	}

	for (SQLSMALLINT col_idx = 1; col_idx <= cols_count; col_idx++) {
		SQLLEN ctype = -1;
		{
			SQLRETURN ret = SQLColAttribute(hstmt, col_idx, SQL_DESC_CONCISE_TYPE, nullptr, 0, nullptr, &ctype);
			if (!SQL_SUCCEEDED(ret)) {
				std::string diag = ReadDiagnostics(hstmt, SQL_HANDLE_STMT);
				throw ScannerException("'SQLColAttribute' for type failed, column index: " + std::to_string(col_idx) +
				                       ", columns count: " + std::to_string(cols_count) + ", query: '" + query +
				                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
			}
		}

		std::vector<SQLCHAR> buf;
		SQLSMALLINT len = 0;
		buf.resize(1024);
		{
			SQLRETURN ret = SQLColAttribute(hstmt, col_idx, SQL_DESC_NAME, buf.data(),
			                                static_cast<SQLSMALLINT>(buf.size()), &len, nullptr);
			if (!SQL_SUCCEEDED(ret)) {
				std::string diag = ReadDiagnostics(hstmt, SQL_HANDLE_STMT);
				throw ScannerException("'SQLColAttribute' for name failed, column index: " + std::to_string(col_idx) +
				                       ", columns count: " + std::to_string(cols_count) + ", query: '" + query +
				                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
			}
		}
		std::string name(reinterpret_cast<char *>(buf.data()), len);

		duckdb_type dtype = OdbcCTypeToDuckType(ctype);
		auto ltype = LogicalTypePtr(duckdb_create_logical_type(dtype), LogicalTypeDeleter);
		duckdb_bind_add_result_column(info, name.c_str(), ltype.get());
	}

	if (cols_count == 0) {
		auto bigint_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_BIGINT), LogicalTypeDeleter);
		duckdb_bind_add_result_column(info, "rowcount", bigint_type.get());
	}

	auto bdata_ptr = std::unique_ptr<BindData>(new BindData(conn, std::move(query), std::move(params), hstmt));
	duckdb_bind_set_bind_data(info, bdata_ptr.release(), BindData::Destroy);
}

static void LocalInit(duckdb_init_info info) {
	BindData &bdata = *reinterpret_cast<BindData *>(duckdb_init_get_bind_data(info));
	auto ctx_ptr = std::unique_ptr<LocalInitData>(new LocalInitData(bdata.conn));
	duckdb_init_set_init_data(info, ctx_ptr.release(), LocalInitData::Destroy);
}

static void Query(duckdb_function_info info, duckdb_data_chunk output) {
	BindData &bdata = *reinterpret_cast<BindData *>(duckdb_function_get_bind_data(info));
	LocalInitData &ldata = *reinterpret_cast<LocalInitData *>(duckdb_function_get_local_init_data(info));

	if (ldata.finished) {
		duckdb_data_chunk_set_size(output, 0);
		return;
	}

	for (size_t i = 0; i < bdata.params.size(); i++) {
		ValuePtr &val = bdata.params.at(i);
		SQLSMALLINT idx = static_cast<SQLSMALLINT>(i + 1);
		SetOdbcParam(bdata.query, bdata.hstmt, val.get(), idx, bdata.params_holder);
	}

	{
		SQLRETURN ret = SQLExecute(bdata.hstmt);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = ReadDiagnostics(bdata.hstmt, SQL_HANDLE_STMT);
			throw ScannerException("'SQLExecute' failed, query: '" + bdata.query + "', return: " + std::to_string(ret) +
			                       ", diagnostics: '" + diag + "'");
		}
	}

	SQLSMALLINT cols_count = 0;
	{
		SQLRETURN ret = SQLNumResultCols(bdata.hstmt, &cols_count);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = ReadDiagnostics(bdata.hstmt, SQL_HANDLE_STMT);
			throw ScannerException("'SQLNumResultCols' failed, query: '" + bdata.query +
			                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
		}
	}

	// DDL or DML query

	if (cols_count == 0) {
		SQLLEN count = -1;
		SQLRETURN ret = SQLRowCount(bdata.hstmt, &count);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = ReadDiagnostics(bdata.hstmt, SQL_HANDLE_STMT);
			throw ScannerException("'SQLRowCount' failed, DDL/DML query: '" + bdata.query +
			                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
		}

		duckdb_vector vec = duckdb_data_chunk_get_vector(output, 0);
		if (vec == nullptr) {
			throw ScannerException("Vector is NULL, DDL/DML query: '" + bdata.query +
			                       "', columns count: " + std::to_string(cols_count));
		}

		int64_t *vec_data = reinterpret_cast<int64_t *>(duckdb_vector_get_data(vec));
		vec_data[0] = static_cast<int64_t>(count);
		duckdb_data_chunk_set_size(output, 1);
		ldata.finished = true;
		return;
	}

	// normal query

	idx_t row_idx = 0;
	for (; row_idx < duckdb_vector_size(); row_idx++) {

		{
			SQLRETURN ret = SQLFetch(bdata.hstmt);
			if (!SQL_SUCCEEDED(ret)) {
				if (ret != SQL_NO_DATA) {
					std::string diag = ReadDiagnostics(bdata.hstmt, SQL_HANDLE_STMT);
					throw ScannerException("'SQLFetch' failed, query: '" + bdata.query +
					                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
				}
				break;
			}
		}

		for (idx_t col_idxz = 0; col_idxz < static_cast<idx_t>(cols_count); col_idxz++) {
			SQLSMALLINT col_idx = static_cast<SQLSMALLINT>(col_idxz + 1);

			duckdb_vector vec = duckdb_data_chunk_get_vector(output, col_idxz);
			if (vec == nullptr) {
				throw ScannerException("Vector is NULL, query: '" + bdata.query + "', columns count: " +
				                       std::to_string(cols_count) + ", column index: " + std::to_string(col_idxz));
			}

			SQLLEN ctype = -1;
			{
				SQLRETURN ret =
				    SQLColAttribute(bdata.hstmt, col_idx, SQL_DESC_CONCISE_TYPE, nullptr, 0, nullptr, &ctype);
				if (!SQL_SUCCEEDED(ret)) {
					std::string diag = ReadDiagnostics(bdata.hstmt, SQL_HANDLE_STMT);
					throw ScannerException(
					    "'SQLColAttribute' for type failed, column index: " + std::to_string(col_idx) +
					    ", columns count: " + std::to_string(cols_count) + ", query: '" + bdata.query +
					    "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
				}
			}

			FetchIntoVector(ctype, bdata.query, bdata.hstmt, col_idx, vec, row_idx);
		}
	}
	duckdb_data_chunk_set_size(output, row_idx);
	ldata.finished = true;
}

static duckdb_state Register(duckdb_connection conn) {
	auto fun = TableFunctionPtr(duckdb_create_table_function(), TableFunctionDeleter);
	duckdb_table_function_set_name(fun.get(), "odbc_query");

	// parameters
	auto bigint_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_BIGINT), LogicalTypeDeleter);
	auto varchar_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_VARCHAR), LogicalTypeDeleter);
	duckdb_table_function_add_parameter(fun.get(), bigint_type.get());
	duckdb_table_function_add_parameter(fun.get(), varchar_type.get());
	duckdb_table_function_add_named_parameter(fun.get(), "params", bigint_type.get());

	// callbacks
	duckdb_table_function_set_bind(fun.get(), odbc_query_bind);
	duckdb_table_function_set_init(fun.get(), odbc_query_init);
	duckdb_table_function_set_local_init(fun.get(), odbc_query_local_init);
	duckdb_table_function_set_function(fun.get(), odbc_query_function);

	// register and cleanup
	duckdb_state state = duckdb_register_table_function(conn, fun.get());

	return state;
}

} // namespace odbcscanner

static void odbc_query_bind(duckdb_bind_info info) noexcept {
	try {
		odbcscanner::Bind(info);
	} catch (std::exception &e) {
		duckdb_bind_set_error(info, e.what());
	}
}

static void odbc_query_init(duckdb_init_info info) noexcept {
	(void)info;
}

static void odbc_query_local_init(duckdb_init_info info) noexcept {
	try {
		odbcscanner::LocalInit(info);
	} catch (std::exception &e) {
		duckdb_init_set_error(info, e.what());
	}
}

static void odbc_query_function(duckdb_function_info info, duckdb_data_chunk output) noexcept {
	try {
		odbcscanner::Query(info, output);
	} catch (std::exception &e) {
		duckdb_function_set_error(info, e.what());
	}
}

duckdb_state odbc_query_register(duckdb_connection conn) /* noexcept */ {
	try {
		return odbcscanner::Register(conn);
	} catch (std::exception &e) {
		(void)e;
		return DuckDBError;
	}
}
