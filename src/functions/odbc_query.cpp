#include "capi_odbc_scanner.h"

#include <memory>
#include <string>
#include <vector>

#include <sql.h>
#include <sqlext.h>

#include "capi_pointers.hpp"
#include "connection.hpp"
#include "defer.hpp"
#include "diagnostics.hpp"
#include "params.hpp"
#include "registries.hpp"
#include "scanner_exception.hpp"
#include "types.hpp"
#include "widechar.hpp"

DUCKDB_EXTENSION_EXTERN

static void odbc_query_bind(duckdb_bind_info info) noexcept;
static void odbc_query_init(duckdb_init_info info) noexcept;
static void odbc_query_local_init(duckdb_init_info info) noexcept;
static void odbc_query_function(duckdb_function_info info, duckdb_data_chunk output) noexcept;

namespace odbcscanner {

struct BindData {
	int64_t conn_id = 0;
	std::string query;
	std::vector<ScannerParam> params;
	int64_t params_handle = 0;
	size_t expected_params_count = 0;
	HSTMT hstmt = SQL_NULL_HSTMT;

	BindData(int64_t conn_id_in, std::string query_in, std::vector<ScannerParam> params_in, int64_t params_handle_in,
	         size_t expected_params_count_in, HSTMT hstmt_in)
	    : conn_id(conn_id_in), query(std::move(query_in)), params(std::move(params_in)),
	      params_handle(params_handle_in), expected_params_count(expected_params_count_in), hstmt(hstmt_in) {
	}

	BindData(const BindData &) = delete;
	BindData(BindData &&) = delete;

	BindData &operator=(const BindData &) = delete;
	BindData &operator=(BindData &&other) = delete;

	~BindData() noexcept {
		SQLFreeStmt(hstmt, SQL_CLOSE);
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);

		if (params_handle != 0) {
			auto params_ptr = ParamsRegistry::Remove(params_handle);
			(void)params_ptr;
		}
	}

	static void Destroy(void *bdata_in) noexcept {
		auto bdata = reinterpret_cast<BindData *>(bdata_in);
		delete bdata;
	}
};

struct LocalInitData {
	std::unique_ptr<OdbcConnection> conn_ptr;
	// todo: enum
	bool finished = false;

	LocalInitData(std::unique_ptr<OdbcConnection> conn_ptr_in) : conn_ptr(std::move(conn_ptr_in)) {
	}

	LocalInitData(const LocalInitData &) = delete;
	LocalInitData(LocalInitData &&) = delete;

	LocalInitData &operator=(const LocalInitData &) = delete;
	LocalInitData &operator=(LocalInitData &&other);

	~LocalInitData() {
		// We are not closing the connection, even in case of error,
		// so need to return connection to registry
		ConnectionsRegistry::Add(std::move(conn_ptr));
	}

	static void Destroy(void *ctx_in) noexcept {
		auto ctx = reinterpret_cast<LocalInitData *>(ctx_in);
		delete ctx;
	}
};

static void Bind(duckdb_bind_info info) {
	auto conn_id_val = ValuePtr(duckdb_bind_get_parameter(info, 0), ValueDeleter);
	if (duckdb_is_null_value(conn_id_val.get())) {
		throw ScannerException("'odbc_query' error: specified ODBC connection must be not NULL");
	}
	int64_t conn_id = duckdb_get_int64(conn_id_val.get());
	auto conn_ptr = ConnectionsRegistry::Remove(conn_id);

	if (conn_ptr.get() == nullptr) {
		throw ScannerException("'odbc_query' error: open ODBC connection not found, id: " + std::to_string(conn_id));
	}

	// Return the connection to registry at the end of the block
	auto deferred = Defer([&conn_ptr] { ConnectionsRegistry::Add(std::move(conn_ptr)); });

	OdbcConnection &conn = *conn_ptr;

	auto query_val = ValuePtr(duckdb_bind_get_parameter(info, 1), ValueDeleter);
	if (duckdb_is_null_value(query_val.get())) {
		throw ScannerException("'odbc_query' error: specified SQL query must be not NULL");
	}
	auto query_ptr = VarcharPtr(duckdb_get_varchar(query_val.get()), VarcharDeleter);
	std::string query(query_ptr.get());

	std::vector<ScannerParam> params;

	auto params_val = ValuePtr(duckdb_bind_get_named_parameter(info, "params"), ValueDeleter);
	if (params_val.get() != nullptr) {
		params = Params::Extract(params_val.get());
	}

	int64_t params_handle = 0;
	auto param_handle_val = ValuePtr(duckdb_bind_get_named_parameter(info, "params_handle"), ValueDeleter);
	if (param_handle_val.get() != nullptr && !duckdb_is_null_value(param_handle_val.get())) {
		params_handle = duckdb_get_int64(param_handle_val.get());
	}

	HSTMT hstmt = SQL_NULL_HSTMT;
	{
		SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, conn.dbc, &hstmt);
		if (!SQL_SUCCEEDED(ret)) {
			throw ScannerException("'SQLAllocHandle' failed for STMT handle, return: " + std::to_string(ret));
		}
	}
	{
		auto wquery = WideChar::Widen(query.data(), query.length());
		SQLRETURN ret = SQLPrepareW(hstmt, wquery.data(), wquery.length<SQLINTEGER>());
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
			throw ScannerException("'SQLPrepare' failed, query: '" + query + "', return: " + std::to_string(ret) +
			                       ", diagnostics: '" + diag + "'");
		}
	}

	size_t expected_params_count = 0;
	if (params_val.get() != nullptr || param_handle_val.get() != nullptr) {
		SQLSMALLINT count = -1;
		SQLRETURN ret = SQLNumParams(hstmt, &count);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
			throw ScannerException("'SQLNumParams' failed, query: '" + query + "', return: " + std::to_string(ret) +
			                       ", diagnostics: '" + diag + "'");
		}
		expected_params_count = static_cast<size_t>(count);
		if (params_val.get() != nullptr && expected_params_count != params.size()) {
			throw ScannerException(
			    "Incorrect number of query parameters specified, expected: " + std::to_string(expected_params_count) +
			    ", actual: " + std::to_string(params.size()) + ", query: '" + query + "'");
		}
	}

	SQLSMALLINT cols_count = -1;
	{
		SQLRETURN ret = SQLNumResultCols(hstmt, &cols_count);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
			throw ScannerException("'SQLNumResultCols' failed, query: '" + query + "', return: " + std::to_string(ret) +
			                       ", diagnostics: '" + diag + "'");
		}
	}

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

		OdbcType odbc_type = Types::GetResultColumnAttributes(query, cols_count, hstmt, col_idx);

		Types::AddResultColumnOfType(info, name, odbc_type);
	}

	if (cols_count == 0) {
		auto bigint_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_BIGINT), LogicalTypeDeleter);
		duckdb_bind_add_result_column(info, "rowcount", bigint_type.get());
	}

	auto bdata_ptr = std::unique_ptr<BindData>(
	    new BindData(conn_id, std::move(query), std::move(params), params_handle, expected_params_count, hstmt));
	duckdb_bind_set_bind_data(info, bdata_ptr.release(), BindData::Destroy);
}

static void LocalInit(duckdb_init_info info) {
	BindData &bdata = *reinterpret_cast<BindData *>(duckdb_init_get_bind_data(info));
	// Keep the connection in local data while the function is running
	auto conn_ptr = ConnectionsRegistry::Remove(bdata.conn_id);
	auto ldata_ptr = std::unique_ptr<LocalInitData>(new LocalInitData(std::move(conn_ptr)));
	duckdb_init_set_init_data(info, ldata_ptr.release(), LocalInitData::Destroy);
}

static void Query(duckdb_function_info info, duckdb_data_chunk output) {
	BindData &bdata = *reinterpret_cast<BindData *>(duckdb_function_get_bind_data(info));
	LocalInitData &ldata = *reinterpret_cast<LocalInitData *>(duckdb_function_get_local_init_data(info));

	if (ldata.finished) {
		duckdb_data_chunk_set_size(output, 0);
		return;
	}

	if (bdata.params.size() > 0) {
		Params::BindToOdbc(bdata.query, bdata.hstmt, bdata.params);
	} else if (bdata.params_handle != 0) {
		auto params_ptr = ParamsRegistry::Remove(bdata.params_handle);
		if (params_ptr.get() == nullptr) {
			throw ScannerException("'odbc_query' error: specified parameters handle not found, ID: " +
			                       std::to_string(bdata.params_handle));
		}
		auto deferred = Defer([&params_ptr] { ParamsRegistry::Add(std::move(params_ptr)); });
		if (bdata.expected_params_count != params_ptr->size()) {
			throw ScannerException("Incorrect number of query parameters specified, expected: " +
			                       std::to_string(bdata.expected_params_count) + ", actual: " +
			                       std::to_string(params_ptr->size()) + ", query: '" + bdata.query + "'");
		}
		Params::BindToOdbc(bdata.query, bdata.hstmt, *params_ptr);
	}

	{
		SQLRETURN ret = SQLExecute(bdata.hstmt);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(bdata.hstmt, SQL_HANDLE_STMT);
			throw ScannerException("'SQLExecute' failed, query: '" + bdata.query + "', return: " + std::to_string(ret) +
			                       ", diagnostics: '" + diag + "'");
		}
	}

	SQLSMALLINT cols_count = 0;
	{
		SQLRETURN ret = SQLNumResultCols(bdata.hstmt, &cols_count);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(bdata.hstmt, SQL_HANDLE_STMT);
			throw ScannerException("'SQLNumResultCols' failed, query: '" + bdata.query +
			                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
		}
	}

	// DDL or DML query

	if (cols_count == 0) {
		SQLLEN count = -1;
		SQLRETURN ret = SQLRowCount(bdata.hstmt, &count);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(bdata.hstmt, SQL_HANDLE_STMT);
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

	std::vector<OdbcType> col_types;
	for (SQLSMALLINT col_idx = 1; col_idx <= cols_count; col_idx++) {
		OdbcType odbc_type = Types::GetResultColumnAttributes(bdata.query, cols_count, bdata.hstmt, col_idx);
		col_types.emplace_back(std::move(odbc_type));
	}

	std::vector<duckdb_vector> col_vectors;
	for (idx_t col_idxz = 0; col_idxz < static_cast<idx_t>(cols_count); col_idxz++) {
		duckdb_vector vec = duckdb_data_chunk_get_vector(output, col_idxz);
		if (vec == nullptr) {
			throw ScannerException("Vector is NULL, query: '" + bdata.query + "', columns count: " +
			                       std::to_string(cols_count) + ", column index: " + std::to_string(col_idxz));
		}
		col_vectors.push_back(vec);
	}

	idx_t row_idx = 0;
	for (; row_idx < duckdb_vector_size(); row_idx++) {

		{
			SQLRETURN ret = SQLFetch(bdata.hstmt);
			if (!SQL_SUCCEEDED(ret)) {
				if (ret != SQL_NO_DATA) {
					std::string diag = Diagnostics::Read(bdata.hstmt, SQL_HANDLE_STMT);
					throw ScannerException("'SQLFetch' failed, query: '" + bdata.query +
					                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
				}
				break;
			}
		}

		for (idx_t col_idxz = 0; col_idxz < static_cast<idx_t>(cols_count); col_idxz++) {
			OdbcType &odbc_type = col_types.at(col_idxz);
			SQLSMALLINT col_idx = static_cast<SQLSMALLINT>(col_idxz + 1);
			duckdb_vector vec = col_vectors.at(col_idxz);

			Types::FetchAndSetResultOfType(odbc_type, bdata.query, bdata.hstmt, col_idx, vec, row_idx);
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
	auto any_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_ANY), LogicalTypeDeleter);
	duckdb_table_function_add_parameter(fun.get(), bigint_type.get());
	duckdb_table_function_add_parameter(fun.get(), varchar_type.get());
	duckdb_table_function_add_named_parameter(fun.get(), "params", any_type.get());
	duckdb_table_function_add_named_parameter(fun.get(), "params_handle", bigint_type.get());

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
