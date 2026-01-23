#include "odbc_scanner.hpp"

#include <memory>
#include <string>
#include <vector>

#include "capi_pointers.hpp"
#include "columns.hpp"
#include "connection.hpp"
#include "dbms_quirks.hpp"
#include "defer.hpp"
#include "diagnostics.hpp"
#include "make_unique.hpp"
#include "odbc_api.hpp"
#include "params.hpp"
#include "query_context.hpp"
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

namespace {

// state of the function call
enum class ExecState { UNINITIALIZED, EXECUTED, EXHAUSTED };

// return status of the SQLExecute call
enum class SqlExecStatus { SUCCESS, FAILURE };

struct QueryOptions {
	bool ignore_exec_failure = false;
	bool close_connection = false;

	QueryOptions(bool ignore_exec_failure_in, bool close_connection_in)
	    : ignore_exec_failure(ignore_exec_failure_in), close_connection(close_connection_in) {
	}
};

struct BindData {
	int64_t conn_id = 0;
	QueryContext ctx;

	std::vector<ResultColumn> columns;

	QueryOptions query_options;

	std::vector<SQLSMALLINT> param_types;
	std::vector<ScannerValue> params;
	int64_t params_handle = 0;

	BindData(int64_t conn_id_in, QueryContext ctx_in, std::vector<ResultColumn> columns_in,
	         QueryOptions query_options_in, std::vector<SQLSMALLINT> param_types_in,
	         std::vector<ScannerValue> params_in, int64_t params_handle_in)
	    : conn_id(conn_id_in), ctx(std::move(ctx_in)), columns(std::move(columns_in)), query_options(query_options_in),
	      param_types(std::move(param_types_in)), params(std::move(params_in)), params_handle(params_handle_in) {
	}

	BindData(const BindData &other) = delete;
	BindData(BindData &&other) = delete;

	BindData &operator=(const BindData &other) = delete;
	BindData &operator=(BindData &&other) = delete;

	~BindData() noexcept {
		SQLFreeStmt(ctx.hstmt, SQL_CLOSE);
		SQLFreeHandle(SQL_HANDLE_STMT, ctx.hstmt);

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

struct GlobalInitData {
	int64_t conn_id;
	std::unique_ptr<OdbcConnection> conn_ptr;
	bool close_connection = false;

	GlobalInitData(int64_t conn_id, std::unique_ptr<OdbcConnection> conn_ptr_in, bool close_connection_in)
	    : conn_id(conn_id), conn_ptr(std::move(conn_ptr_in)), close_connection(close_connection_in) {
		if (!conn_ptr) {
			throw ScannerException("'odbc_query' error: ODBC connection not found on global init, id: " +
			                       std::to_string(conn_id));
		}
	}

	~GlobalInitData() {
		if (!close_connection) {
			// We are not closing the connection, even in case of error,
			// so need to return connection to registry
			ConnectionsRegistry::Add(std::move(conn_ptr));
		}
	}

	static void Destroy(void *gdata_in) noexcept {
		auto gdata = reinterpret_cast<GlobalInitData *>(gdata_in);
		delete gdata;
	}
};

struct LocalInitData {
	ExecState exec_state = ExecState::UNINITIALIZED;

	LocalInitData() {
	}

	static void Destroy(void *ldata_in) noexcept {
		auto ldata = reinterpret_cast<LocalInitData *>(ldata_in);
		delete ldata;
	}
};

} // namespace

static std::map<std::string, ValuePtr> ExtractUserQuirks(duckdb_bind_info info) {
	std::map<std::string, ValuePtr> res;
	for (auto &name : DbmsQuirks::AllNames()) {
		auto val = ValuePtr(duckdb_bind_get_named_parameter(info, name.c_str()), ValueDeleter);
		res.emplace(name, std::move(val));
	}
	return res;
}

static QueryOptions ExtractQueryOptions(duckdb_value ignore_exec_failure_val, duckdb_value close_connection_val,
                                        bool conn_must_be_closed) {
	bool ignore_exec_failure = false;
	if (ignore_exec_failure_val != nullptr && !duckdb_is_null_value(ignore_exec_failure_val)) {
		ignore_exec_failure = duckdb_get_bool(ignore_exec_failure_val);
	}
	bool close_connection = conn_must_be_closed;
	if (close_connection_val != nullptr && !duckdb_is_null_value(close_connection_val)) {
		close_connection = duckdb_get_bool(close_connection_val);
		if (conn_must_be_closed && !close_connection) {
			throw ScannerException("'odbc_query' error: 'close_connection=FALSE' option cannot be specified along with "
			                       "a connection string");
		}
	}
	return QueryOptions(ignore_exec_failure, close_connection);
}

static void Bind(duckdb_bind_info info) {
	auto conn_id_or_str_val = ValuePtr(duckdb_bind_get_parameter(info, 0), ValueDeleter);
	auto extracted_conn = OdbcConnection::ExtractOrOpen("odbc_query", conn_id_or_str_val.get());
	// Return the connection to registry at the end of the block
	auto deferred = Defer([&extracted_conn] { ConnectionsRegistry::Add(std::move(extracted_conn.ptr)); });
	OdbcConnection &conn = *extracted_conn.ptr;

	auto query_val = ValuePtr(duckdb_bind_get_parameter(info, 1), ValueDeleter);
	if (duckdb_is_null_value(query_val.get())) {
		throw ScannerException("'odbc_query' error: specified SQL query must be not NULL");
	}
	auto query_ptr = VarcharPtr(duckdb_get_varchar(query_val.get()), VarcharDeleter);
	std::string query(query_ptr.get());

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

	auto ignore_exec_failure_val = ValuePtr(duckdb_bind_get_named_parameter(info, "ignore_exec_failure"), ValueDeleter);
	auto close_connection_val = ValuePtr(duckdb_bind_get_named_parameter(info, "close_connection"), ValueDeleter);
	QueryOptions query_options =
	    ExtractQueryOptions(ignore_exec_failure_val.get(), close_connection_val.get(), extracted_conn.must_be_closed);

	std::map<std::string, ValuePtr> user_quirks = ExtractUserQuirks(info);
	DbmsQuirks quirks(conn, user_quirks);
	QueryContext ctx(query, hstmt, quirks);

	std::vector<ScannerValue> params;
	auto params_val = ValuePtr(duckdb_bind_get_named_parameter(info, "params"), ValueDeleter);
	if (params_val.get() != nullptr) {
		params = Params::Extract(quirks, params_val.get());
	}

	int64_t params_handle = 0;
	auto param_handle_val = ValuePtr(duckdb_bind_get_named_parameter(info, "params_handle"), ValueDeleter);
	if (param_handle_val.get() != nullptr && !duckdb_is_null_value(param_handle_val.get())) {
		params_handle = duckdb_get_int64(param_handle_val.get());
		auto params_ptr = ParamsRegistry::Remove(params_handle);
		if (params_ptr.get() == nullptr) {
			throw ScannerException("'odbc_query' error: specified parameters handle not found, ID: " +
			                       std::to_string(params_handle));
		}
		ParamsRegistry::Add(std::move(params_ptr));
	}

	std::vector<ResultColumn> columns = Columns::Collect(ctx);

	// This must go after the Columns::Collect to prevent the crash in MSSQL ODBC driver
	// https://web.archive.org/web/20251225082810/https://developercommunity.visualstudio.com/t/SQL-Server-ODBC-driver-186-crash-with-S/11021276
	std::vector<SQLSMALLINT> param_types;
	if (params_val.get() != nullptr || param_handle_val.get() != nullptr) {
		param_types = Params::CollectTypes(ctx);
		if (params_val.get() != nullptr) {
			Params::SetExpectedTypes(ctx, param_types, params);
		}
	}

	if (columns.size() == 0) {
		auto bigint_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_BIGINT), LogicalTypeDeleter);
		duckdb_bind_add_result_column(info, "rowcount", bigint_type.get());
	} else {
		for (ResultColumn &col : columns) {
			Types::CoalesceColumnType(ctx, col);
			duckdb_type type_id = Types::ResolveColumnType(ctx, col);
			Columns::AddToResults(info, type_id, col);
		}
	}

	auto bdata_ptr =
	    std::unique_ptr<BindData>(new BindData(extracted_conn.id, std::move(ctx), std::move(columns), query_options,
	                                           std::move(param_types), std::move(params), params_handle));
	duckdb_bind_set_bind_data(info, bdata_ptr.release(), BindData::Destroy);
}

static void GlobalInit(duckdb_init_info info) {
	BindData &bdata = *reinterpret_cast<BindData *>(duckdb_init_get_bind_data(info));
	// Keep the connection in global data while the function is running
	// to not allow other threads operate on it or close it.
	auto conn_ptr = ConnectionsRegistry::Remove(bdata.conn_id);
	auto gdata_ptr =
	    std_make_unique<GlobalInitData>(bdata.conn_id, std::move(conn_ptr), bdata.query_options.close_connection);
	duckdb_init_set_init_data(info, gdata_ptr.release(), GlobalInitData::Destroy);
}

static void LocalInit(duckdb_init_info info) {
	auto ldata_ptr = std_make_unique<LocalInitData>();
	duckdb_init_set_init_data(info, ldata_ptr.release(), LocalInitData::Destroy);
}

static SqlExecStatus BindParamsAndExecute(BindData &bdata) {
	QueryContext &ctx = bdata.ctx;

	if (bdata.params.size() > 0) {
		Params::BindToOdbc(ctx, bdata.params);
	} else if (bdata.params_handle != 0) {
		auto params_ptr = ParamsRegistry::Remove(bdata.params_handle);
		if (params_ptr.get() == nullptr) {
			throw ScannerException("'odbc_query' error: specified parameters handle not found, ID: " +
			                       std::to_string(bdata.params_handle));
		}
		auto deferred = Defer([&params_ptr] { ParamsRegistry::Add(std::move(params_ptr)); });
		Params::SetExpectedTypes(bdata.ctx, bdata.param_types, *params_ptr);
		Params::BindToOdbc(ctx, *params_ptr);
	}

	if (ctx.quirks.reset_stmt_before_execute) {
		SQLRETURN ret = SQLFreeStmt(ctx.hstmt, SQL_CLOSE);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
			throw ScannerException("'SQLFreeStmt' with SQL_CLOSE (reset_stmt_before_execute) failed, query: '" +
			                       ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
		}
	}

	{
		SQLRETURN ret = SQLExecute(ctx.hstmt);
		if (!SQL_SUCCEEDED(ret)) {
			if (bdata.query_options.ignore_exec_failure) {
				return SqlExecStatus::FAILURE;
			}
			std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
			throw ScannerException("'SQLExecute' failed, query: '" + ctx.query + "', return: " + std::to_string(ret) +
			                       ", diagnostics: '" + diag + "'");
		}
	}

	return SqlExecStatus::SUCCESS;
}

static void Query(duckdb_function_info info, duckdb_data_chunk output) {
	BindData &bdata = *reinterpret_cast<BindData *>(duckdb_function_get_bind_data(info));
	LocalInitData &ldata = *reinterpret_cast<LocalInitData *>(duckdb_function_get_local_init_data(info));
	QueryContext &ctx = bdata.ctx;

	if (ldata.exec_state == ExecState::EXHAUSTED) {
		duckdb_data_chunk_set_size(output, 0);
		return;
	}

	if (ldata.exec_state == ExecState::UNINITIALIZED) {
		// run the query
		SqlExecStatus exec_status = BindParamsAndExecute(bdata);

		// if exec error is not thrown then we return empty result set
		if (exec_status == SqlExecStatus::FAILURE) {
			ldata.exec_state = ExecState::EXHAUSTED;
			duckdb_data_chunk_set_size(output, 0);
			return;
		}

		ldata.exec_state = ExecState::EXECUTED;

		std::vector<ResultColumn> columns = Columns::Collect(ctx);
		Columns::CheckSame(ctx, bdata.columns, columns);

		// DDL or DML query

		if (bdata.columns.size() == 0) {
			SQLLEN count = -1;
			SQLRETURN ret = SQLRowCount(ctx.hstmt, &count);
			if (!SQL_SUCCEEDED(ret)) {
				std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
				throw ScannerException("'SQLRowCount' failed, DDL/DML query: '" + ctx.query +
				                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
			}

			duckdb_vector vec = duckdb_data_chunk_get_vector(output, 0);
			if (vec == nullptr) {
				throw ScannerException("Vector is NULL, DDL/DML query: '" + ctx.query +
				                       "', columns count: " + std::to_string(columns.size()));
			}

			int64_t *vec_data = reinterpret_cast<int64_t *>(duckdb_vector_get_data(vec));
			vec_data[0] = static_cast<int64_t>(count);
			duckdb_data_chunk_set_size(output, 1);
			ldata.exec_state = ExecState::EXHAUSTED;
			return;
		}

		// normal query

		ctx.col_binds.resize(bdata.columns.size());
		for (idx_t col_idxz = 0; col_idxz < static_cast<idx_t>(bdata.columns.size()); col_idxz++) {
			// set column descriptors and bind columns
			ResultColumn &col = bdata.columns.at(col_idxz);
			SQLSMALLINT col_idx = static_cast<SQLSMALLINT>(col_idxz + 1);
			Types::BindColumn(ctx, col.odbc_type, col_idx);
		}
	}

	// collect vectors
	std::vector<duckdb_vector> col_vectors;
	for (idx_t col_idxz = 0; col_idxz < static_cast<idx_t>(bdata.columns.size()); col_idxz++) {
		duckdb_vector vec = duckdb_data_chunk_get_vector(output, col_idxz);
		if (vec == nullptr) {
			throw ScannerException("Vector is NULL, query: '" + ctx.query +
			                       "', columns count: " + std::to_string(bdata.columns.size()) +
			                       ", column index: " + std::to_string(col_idxz));
		}
		col_vectors.push_back(vec);
	}

	idx_t row_idx = 0;
	for (; row_idx < duckdb_vector_size(); row_idx++) {
		{
			SQLRETURN ret = SQLFetch(ctx.hstmt);
			if (!SQL_SUCCEEDED(ret)) {
				if (ret != SQL_NO_DATA) {
					std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
					throw ScannerException("'SQLFetch' failed, query: '" + ctx.query +
					                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
				}
				SQLRETURN ret_close = SQLFreeStmt(ctx.hstmt, SQL_CLOSE);
				if (!SQL_SUCCEEDED(ret_close)) {
					std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
					throw ScannerException("'SQLFreeStmt' with SQL_CLOSE failed, query: '" + ctx.query +
					                       "', return: " + std::to_string(ret_close) + ", diagnostics: '" + diag + "'");
				}
				ldata.exec_state = ExecState::EXHAUSTED;
				break;
			}
		}

		for (idx_t col_idxz = 0; col_idxz < static_cast<idx_t>(bdata.columns.size()); col_idxz++) {
			ResultColumn &col = bdata.columns.at(col_idxz);
			duckdb_vector vec = col_vectors.at(col_idxz);
			SQLSMALLINT col_idx = static_cast<SQLSMALLINT>(col_idxz + 1);

			Types::FetchAndSetResult(ctx, col.odbc_type, col_idx, vec, row_idx);
		}
	}
	duckdb_data_chunk_set_size(output, row_idx);
}

void OdbcQueryFunction::Register(duckdb_connection conn) {
	auto fun = TableFunctionPtr(duckdb_create_table_function(), TableFunctionDeleter);
	duckdb_table_function_set_name(fun.get(), "odbc_query");

	// parameters
	auto bigint_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_BIGINT), LogicalTypeDeleter);
	auto varchar_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_VARCHAR), LogicalTypeDeleter);
	auto any_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_ANY), LogicalTypeDeleter);
	auto bool_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_BOOLEAN), LogicalTypeDeleter);
	auto utinyint_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_UTINYINT), LogicalTypeDeleter);
	auto uint_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_UINTEGER), LogicalTypeDeleter);
	duckdb_table_function_add_parameter(fun.get(), any_type.get());
	duckdb_table_function_add_parameter(fun.get(), varchar_type.get());
	// named args
	duckdb_table_function_add_named_parameter(fun.get(), "ignore_exec_failure", bool_type.get());
	duckdb_table_function_add_named_parameter(fun.get(), "close_connection", bool_type.get());
	// query params
	duckdb_table_function_add_named_parameter(fun.get(), "params", any_type.get());
	duckdb_table_function_add_named_parameter(fun.get(), "params_handle", bigint_type.get());
	// quirks
	duckdb_table_function_add_named_parameter(fun.get(), "decimal_columns_as_chars", bool_type.get());
	duckdb_table_function_add_named_parameter(fun.get(), "decimal_columns_precision_through_ard", bool_type.get());
	duckdb_table_function_add_named_parameter(fun.get(), "decimal_columns_precision_through_ard_bind", bool_type.get());
	duckdb_table_function_add_named_parameter(fun.get(), "decimal_params_as_chars", bool_type.get());
	duckdb_table_function_add_named_parameter(fun.get(), "integral_params_as_decimals", bool_type.get());
	duckdb_table_function_add_named_parameter(fun.get(), "reset_stmt_before_execute", bool_type.get());
	duckdb_table_function_add_named_parameter(fun.get(), "time_params_as_ss_time2", bool_type.get());
	duckdb_table_function_add_named_parameter(fun.get(), "timestamp_columns_as_timestamp_ns", bool_type.get());
	duckdb_table_function_add_named_parameter(fun.get(), "timestamp_columns_with_typename_date_as_date",
	                                          bool_type.get());
	duckdb_table_function_add_named_parameter(fun.get(), "timestamp_max_fraction_precision", utinyint_type.get());
	duckdb_table_function_add_named_parameter(fun.get(), "timestamp_params_as_sf_timestamp_ntz", bool_type.get());
	duckdb_table_function_add_named_parameter(fun.get(), "timestamptz_params_as_ss_timestampoffset", bool_type.get());
	duckdb_table_function_add_named_parameter(fun.get(), "var_len_data_single_part", bool_type.get());
	duckdb_table_function_add_named_parameter(fun.get(), "var_len_params_long_threshold_bytes", uint_type.get());

	// callbacks
	duckdb_table_function_set_bind(fun.get(), odbc_query_bind);
	duckdb_table_function_set_init(fun.get(), odbc_query_init);
	duckdb_table_function_set_local_init(fun.get(), odbc_query_local_init);
	duckdb_table_function_set_function(fun.get(), odbc_query_function);

	// register and cleanup
	duckdb_state state = duckdb_register_table_function(conn, fun.get());

	if (state != DuckDBSuccess) {
		throw ScannerException("'odbc_query' function registration failed");
	}
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
	try {
		odbcscanner::GlobalInit(info);
	} catch (std::exception &e) {
		duckdb_init_set_error(info, e.what());
	}
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
