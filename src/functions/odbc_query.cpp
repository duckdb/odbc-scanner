#include "odbc_scanner.hpp"

#include <memory>
#include <string>
#include <vector>

#include "function_args.hpp"
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

static void odbc_query_bind(duckdb::cxx::TableFunction::BindInput &input);
static void odbc_query_init(duckdb::cxx::TableFunction::InitGlobalInput &input);
static void odbc_query_local_init(duckdb::cxx::TableFunction::InitLocalInput &input);
static void odbc_query_function(duckdb::cxx::TableFunction::ExecInput &input);

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
	// param buffers are bound to the statement during the execution
	mutable std::vector<ScannerValue> params;

	BindData(int64_t conn_id_in, QueryContext ctx_in, std::vector<ResultColumn> columns_in,
	         QueryOptions query_options_in, std::vector<SQLSMALLINT> param_types_in,
	         std::vector<ScannerValue> params_in)
	    : conn_id(conn_id_in), ctx(std::move(ctx_in)), columns(std::move(columns_in)), query_options(query_options_in),
	      param_types(std::move(param_types_in)), params(std::move(params_in)) {
	}

	BindData(const BindData &other) = delete;
	BindData(BindData &&other) = delete;

	BindData &operator=(const BindData &other) = delete;
	BindData &operator=(BindData &&other) = delete;
};

struct GlobalInitData {
	int64_t conn_id;
	std::unique_ptr<OdbcConnection> conn_ptr;
	bool close_connection = false;

	GlobalInitData(int64_t conn_id, std::unique_ptr<OdbcConnection> conn_ptr_in, bool close_connection_in, HSTMT stmt)
	    : conn_id(conn_id), conn_ptr(std::move(conn_ptr_in)), close_connection(close_connection_in) {
		if (!conn_ptr) {
			throw ScannerException("'odbc_query' error: ODBC connection not found on global init, id: " +
			                       std::to_string(conn_id));
		}
		CancellationRegistry::Add(conn_id, stmt);
	}

	~GlobalInitData() {
		CancellationRegistry::Remove(conn_id);
		if (!close_connection) {
			// We are not closing the connection, even in case of error,
			// so need to return connection to registry
			ConnectionsRegistry::Add(std::move(conn_ptr));
		}
	}
};

struct LocalInitData {
	ExecState exec_state = ExecState::UNINITIALIZED;

	LocalInitData() {
	}
};

} // namespace

static QueryOptions ExtractQueryOptions(const Value *ignore_exec_failure_val, const Value *close_connection_val,
                                        bool conn_must_be_closed) {
	bool ignore_exec_failure = false;
	if (ignore_exec_failure_val != nullptr && !ignore_exec_failure_val->IsNull()) {
		ignore_exec_failure = ignore_exec_failure_val->Get<bool>();
	}
	bool close_connection = conn_must_be_closed;
	if (close_connection_val != nullptr && !close_connection_val->IsNull()) {
		close_connection = close_connection_val->Get<bool>();
		if (conn_must_be_closed && !close_connection) {
			throw ScannerException("'odbc_query' error: 'close_connection=FALSE' option cannot be specified along with "
			                       "a connection string");
		}
	}
	return QueryOptions(ignore_exec_failure, close_connection);
}

static void Bind(TableFunction::BindInput &input) {
	Factory &factory = input.GetContext().GetFactory();
	auto conn_id_or_str_val = FunctionArgs::GetArg(input, 0);
	auto extracted_conn = OdbcConnection::ExtractOrOpen("odbc_query", conn_id_or_str_val.get());
	// Return the connection to registry at the end of the block
	auto deferred = Defer([&extracted_conn] { ConnectionsRegistry::Add(std::move(extracted_conn.ptr)); });
	OdbcConnection &conn = *extracted_conn.ptr;

	auto query_val = FunctionArgs::GetArg(input, 1);
	if (query_val->IsNull()) {
		throw ScannerException("'odbc_query' error: specified SQL query must be not NULL");
	}
	std::string query = std::string(query_val->Get<varchar_t>().view());

	StmtHandlePtr hstmt(nullptr, StmtHandleDeleter);
	{
		HSTMT hstmt_out = SQL_NULL_HSTMT;
		SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, conn.dbc, &hstmt_out);
		if (!SQL_SUCCEEDED(ret)) {
			throw ScannerException("'SQLAllocHandle' failed for STMT handle, return: " + std::to_string(ret));
		}
		hstmt.reset(hstmt_out);
	}
	{
		auto wquery = WideChar::Widen(query.data(), query.length());
		SQLRETURN ret = SQLPrepareW(hstmt.get(), wquery.data(), wquery.length<SQLINTEGER>());
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(hstmt.get(), SQL_HANDLE_STMT);
			throw ScannerException("'SQLPrepare' failed, query: '" + query + "', return: " + std::to_string(ret) +
			                       ", diagnostics: '" + diag + "'");
		}
	}

	auto ignore_exec_failure_val = FunctionArgs::GetNamedArg(input, "ignore_exec_failure");
	auto close_connection_val = FunctionArgs::GetNamedArg(input, "close_connection");
	QueryOptions query_options =
	    ExtractQueryOptions(ignore_exec_failure_val.get(), close_connection_val.get(), extracted_conn.must_be_closed);

	std::map<std::string, std::unique_ptr<Value>> user_quirks = DbmsQuirks::ExtractUserQuirks(input);
	DbmsQuirks quirks(conn, user_quirks);
	QueryContext ctx(query, std::move(hstmt), quirks);

	std::vector<ScannerValue> params;
	auto params_val = FunctionArgs::GetNamedArg(input, "params");
	if (params_val.get() != nullptr) {
		params = Params::Extract(quirks, params_val.get());
	}

	std::vector<ResultColumn> columns = Columns::Collect(ctx);

	// This must go after the Columns::Collect to prevent the crash in MSSQL ODBC driver
	// https://web.archive.org/web/20251225082810/https://developercommunity.visualstudio.com/t/SQL-Server-ODBC-driver-186-crash-with-S/11021276
	std::vector<SQLSMALLINT> param_types;
	if (params_val.get() != nullptr) {
		param_types = Params::CollectTypes(ctx);
		Params::SetExpectedTypes(ctx, param_types, params);
	}

	if (columns.size() == 0) {
		auto bigint_type = factory.CreateType(LogicalTypeId::BIGINT);
		input.AddResultColumn("rowcount", bigint_type);
	} else {
		for (ResultColumn &col : columns) {
			Types::CoalesceColumnType(ctx, col);
			LogicalTypeId type_id = Types::ResolveColumnType(ctx, col);
			Columns::AddToResults(input, factory, type_id, col);
		}
	}

	input.SetBindData<BindData>(extracted_conn.id, std::move(ctx), std::move(columns), query_options,
	                            std::move(param_types), std::move(params));
}

static void GlobalInit(TableFunction::InitGlobalInput &input) {
	const BindData &bdata = input.GetBindData<BindData>();
	// Keep the connection in global data while the function is running
	// to not allow other threads operate on it or close it.
	auto conn_ptr = ConnectionsRegistry::Remove(bdata.conn_id);
	input.SetGlobalState<GlobalInitData>(bdata.conn_id, std::move(conn_ptr), bdata.query_options.close_connection,
	                                     bdata.ctx.hstmt());
}

static void LocalInit(TableFunction::InitLocalInput &input) {
	input.SetLocalState<LocalInitData>();
}

static SqlExecStatus BindParamsAndExecute(const BindData &bdata) {
	const QueryContext &ctx = bdata.ctx;

	if (bdata.params.size() > 0) {
		Params::BindToOdbc(ctx, bdata.params);
	}

	if (ctx.quirks.reset_stmt_before_execute) {
		SQLRETURN ret = SQLFreeStmt(ctx.hstmt(), SQL_CLOSE);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
			throw ScannerException("'SQLFreeStmt' with SQL_CLOSE (reset_stmt_before_execute) failed, query: '" +
			                       ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
		}
	}

	{
		SQLRETURN ret = SQLExecute(ctx.hstmt());
		if (!SQL_SUCCEEDED(ret) && ret != SQL_NO_DATA) {
			if (bdata.query_options.ignore_exec_failure) {
				return SqlExecStatus::FAILURE;
			}
			std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
			throw ScannerException("'SQLExecute' failed, query: '" + ctx.query + "', return: " + std::to_string(ret) +
			                       ", diagnostics: '" + diag + "'");
		}
	}

	return SqlExecStatus::SUCCESS;
}

static void Query(TableFunction::ExecInput &input, DataChunk &output) {
	const BindData &bdata = input.GetBindData<BindData>();
	LocalInitData &ldata = input.GetLocalState<LocalInitData>();
	const QueryContext &ctx = bdata.ctx;

	if (ldata.exec_state == ExecState::EXHAUSTED) {
		output.GetVector(0).SetSize(0);
		return;
	}

	if (ldata.exec_state == ExecState::UNINITIALIZED) {
		// run the query
		SqlExecStatus exec_status = BindParamsAndExecute(bdata);

		// if exec error is not thrown then we return empty result set
		if (exec_status == SqlExecStatus::FAILURE) {
			ldata.exec_state = ExecState::EXHAUSTED;
			output.GetVector(0).SetSize(0);
			return;
		}

		ldata.exec_state = ExecState::EXECUTED;

		std::vector<ResultColumn> columns = Columns::Collect(ctx);
		Columns::CheckSame(ctx, bdata.columns, columns);

		// DDL or DML query

		if (bdata.columns.size() == 0) {
			SQLLEN count = -1;
			SQLRETURN ret = SQLRowCount(ctx.hstmt(), &count);
			if (!SQL_SUCCEEDED(ret)) {
				std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
				throw ScannerException("'SQLRowCount' failed, DDL/DML query: '" + ctx.query +
				                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
			}

			Vector vec = output.GetVector(0);
			if (!vec) {
				throw ScannerException("Vector is NULL, DDL/DML query: '" + ctx.query +
				                       "', columns count: " + std::to_string(columns.size()));
			}

			int64_t *vec_data = vec.GetDataMutable<int64_t>();
			vec_data[0] = static_cast<int64_t>(count);
			output.GetVector(0).SetSize(1);
			ldata.exec_state = ExecState::EXHAUSTED;
			return;
		}

		// normal query

		ctx.col_binds.resize(bdata.columns.size());
		for (idx_t col_idxz = 0; col_idxz < static_cast<idx_t>(bdata.columns.size()); col_idxz++) {
			// set column descriptors and bind columns
			const ResultColumn &col = bdata.columns.at(col_idxz);
			SQLSMALLINT col_idx = static_cast<SQLSMALLINT>(col_idxz + 1);
			Types::BindColumn(ctx, col.odbc_type, col_idx);
		}
	}

	// collect vectors
	std::vector<Vector> col_vectors;
	for (idx_t col_idxz = 0; col_idxz < static_cast<idx_t>(bdata.columns.size()); col_idxz++) {
		Vector vec = output.GetVector(col_idxz);
		if (!vec) {
			throw ScannerException("Vector is NULL, query: '" + ctx.query +
			                       "', columns count: " + std::to_string(bdata.columns.size()) +
			                       ", column index: " + std::to_string(col_idxz));
		}
		col_vectors.emplace_back(std::move(vec));
	}

	idx_t vector_size = output.GetCapacity();
	idx_t row_idx = 0;
	for (; row_idx < vector_size; row_idx++) {
		{
			SQLRETURN ret = SQLFetch(ctx.hstmt());
			if (!SQL_SUCCEEDED(ret)) {
				if (ret != SQL_NO_DATA) {
					std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
					throw ScannerException("'SQLFetch' failed, query: '" + ctx.query +
					                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
				}
				SQLRETURN ret_close = SQLFreeStmt(ctx.hstmt(), SQL_CLOSE);
				if (!SQL_SUCCEEDED(ret_close)) {
					std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
					throw ScannerException("'SQLFreeStmt' with SQL_CLOSE failed, query: '" + ctx.query +
					                       "', return: " + std::to_string(ret_close) + ", diagnostics: '" + diag + "'");
				}
				ldata.exec_state = ExecState::EXHAUSTED;
				break;
			}
		}

		for (idx_t col_idxz = 0; col_idxz < static_cast<idx_t>(bdata.columns.size()); col_idxz++) {
			const ResultColumn &col = bdata.columns.at(col_idxz);
			Vector &vec = col_vectors.at(col_idxz);
			SQLSMALLINT col_idx = static_cast<SQLSMALLINT>(col_idxz + 1);

			Types::FetchAndSetResult(ctx, col.odbc_type, col_idx, vec, row_idx);
		}
	}
	output.GetVector(0).SetSize(row_idx);
}

void OdbcQueryFunction::Register(Extension &extension, Factory &factory) {
	auto fun = TableFunction::Create(factory);
	fun.SetName("odbc_query");
	FunctionSignature sig = fun.GetSignature();

	// parameters
	auto varchar_type = factory.CreateType(LogicalTypeId::VARCHAR);
	auto any_type = factory.CreateType(LogicalTypeId::ANY);
	auto bool_type = factory.CreateType(LogicalTypeId::BOOLEAN);
	auto utinyint_type = factory.CreateType(LogicalTypeId::UTINYINT);
	auto uint_type = factory.CreateType(LogicalTypeId::UINTEGER);
	sig.AddParameter("connection", any_type, FunctionParameterKind::POSITIONAL_ONLY);
	sig.AddParameter("query", varchar_type, FunctionParameterKind::POSITIONAL_ONLY);
	// named args
	FunctionArgs::AddNamedParameter(factory, sig, "ignore_exec_failure", bool_type);
	FunctionArgs::AddNamedParameter(factory, sig, "close_connection", bool_type);
	// query params
	FunctionArgs::AddNamedParameter(factory, sig, "params", any_type);
	// quirks
	FunctionArgs::AddNamedParameter(factory, sig, "decimal_columns_as_chars", bool_type);
	FunctionArgs::AddNamedParameter(factory, sig, "decimal_columns_precision_through_ard", bool_type);
	FunctionArgs::AddNamedParameter(factory, sig, "decimal_columns_as_ard_type", bool_type);
	FunctionArgs::AddNamedParameter(factory, sig, "decimal_params_as_chars", bool_type);
	FunctionArgs::AddNamedParameter(factory, sig, "integral_params_as_decimals", bool_type);
	FunctionArgs::AddNamedParameter(factory, sig, "reset_stmt_before_execute", bool_type);
	FunctionArgs::AddNamedParameter(factory, sig, "time_params_as_ss_time2", bool_type);
	FunctionArgs::AddNamedParameter(factory, sig, "timestamp_columns_as_timestamp_ns", bool_type);
	FunctionArgs::AddNamedParameter(factory, sig, "timestamp_columns_with_typename_date_as_date", bool_type);
	FunctionArgs::AddNamedParameter(factory, sig, "timestamp_max_fraction_precision", utinyint_type);
	FunctionArgs::AddNamedParameter(factory, sig, "timestamp_params_as_sf_timestamp_ntz", bool_type);
	FunctionArgs::AddNamedParameter(factory, sig, "timestamptz_params_as_ss_timestampoffset", bool_type);
	FunctionArgs::AddNamedParameter(factory, sig, "var_len_data_single_part", bool_type);
	FunctionArgs::AddNamedParameter(factory, sig, "var_len_params_long_threshold_bytes", uint_type);
	FunctionArgs::AddNamedParameter(factory, sig, "enable_columns_binding", bool_type);

	// callbacks
	fun.SetBindCallback(odbc_query_bind);
	fun.SetInitGlobalCallback(odbc_query_init);
	fun.SetInitLocalCallback(odbc_query_local_init);
	fun.SetExecCallback(odbc_query_function);

	// register and cleanup
	try {
		extension.Register(fun);
	} catch (std::exception &e) {
		throw ScannerException(std::string("\'") + "odbc_query" + "\' function registration failed: " + e.what());
	}
}

} // namespace odbcscanner

static void odbc_query_bind(duckdb::cxx::TableFunction::BindInput &input) {
	try {
		odbcscanner::Bind(input);
	} catch (std::exception &e) {
		throw odbcscanner::FunctionArgs::BinderError(e.what());
	}
}

static void odbc_query_init(duckdb::cxx::TableFunction::InitGlobalInput &input) {
	odbcscanner::GlobalInit(input);
}

static void odbc_query_local_init(duckdb::cxx::TableFunction::InitLocalInput &input) {
	odbcscanner::LocalInit(input);
}

static void odbc_query_function(duckdb::cxx::TableFunction::ExecInput &input) {
	duckdb::cxx::DataChunk output = input.GetOutputChunk();
	odbcscanner::Query(input, output);
}
