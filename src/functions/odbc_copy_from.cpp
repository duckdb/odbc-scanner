#include "odbc_scanner.hpp"

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "capi_pointers.hpp"
#include "dbms_quirks.hpp"
#include "diagnostics.hpp"
#include "duckdb_extension_api.hpp"
#include "make_unique.hpp"
#include "odbc_api.hpp"
#include "query_context.hpp"
#include "registries.hpp"
#include "scanner_exception.hpp"
#include "scanner_value.hpp"
#include "strings.hpp"
#include "types.hpp"
#include "widechar.hpp"

DUCKDB_EXTENSION_EXTERN

static void odbc_copy_from_bind(duckdb_bind_info info) noexcept;
static void odbc_copy_from_init(duckdb_init_info info) noexcept;
static void odbc_copy_from_local_init(duckdb_init_info info) noexcept;
static void odbc_copy_from_function(duckdb_function_info info, duckdb_data_chunk output) noexcept;

namespace odbcscanner {

namespace {

enum class ExecState { UNINITIALIZED, EXECUTED, EXHAUSTED };

struct BindData {
	int64_t conn_id = 0;
	std::string table_name;
	std::string file_path;

	BindData(int64_t conn_id_in, std::string table_name_in, std::string file_path_in)
	    : conn_id(conn_id_in), table_name(std::move(table_name_in)), file_path(std::move(file_path_in)) {
	}

	static void Destroy(void *bdata_in) noexcept {
		auto bdata = reinterpret_cast<BindData *>(bdata_in);
		delete bdata;
	}
};

struct GlobalInitData {
	int64_t conn_id;
	std::unique_ptr<OdbcConnection> conn_ptr;

	GlobalInitData(int64_t conn_id, std::unique_ptr<OdbcConnection> conn_ptr_in)
	    : conn_id(conn_id), conn_ptr(std::move(conn_ptr_in)) {
		if (!conn_ptr) {
			throw ScannerException("'odbc_copy_from' error: ODBC connection not found on global init, id: " +
			                       std::to_string(conn_id));
		}
	}

	~GlobalInitData() {
		// We are not closing the connection, even in case of error,
		// so need to return connection to registry
		ConnectionsRegistry::Add(std::move(conn_ptr));
	}

	static void Destroy(void *gdata_in) noexcept {
		auto gdata = reinterpret_cast<GlobalInitData *>(gdata_in);
		delete gdata;
	}
};

struct FileColumn {
	std::string name;
	duckdb_type dtype;

	FileColumn();

	FileColumn(std::string name_in, duckdb_type dtype_in) : name(std::move(name_in)), dtype(dtype_in) {
	}
};

struct FileReader {
	DatabasePtr db;
	ConnectionPtr conn;
	duckdb_result result;
	ResultPtr res_ptr;
	DataChunkPtr chunk;
	idx_t chunk_size;
	idx_t row_idx = 0;
	std::vector<FileColumn> columns;
	std::vector<duckdb_vector> vectors;
	std::vector<uint64_t *> validities;

	FileReader(DatabasePtr db_in, ConnectionPtr conn_in, duckdb_result result_in, DataChunkPtr chunk_in,
	           std::vector<FileColumn> columns_in)
	    : db(std::move(db_in)), conn(std::move(conn_in)), result(result_in), res_ptr(nullptr, ResultDeleter),
	      chunk(std::move(chunk_in)), columns(std::move(columns_in)) {
		this->res_ptr = ResultPtr(&result, ResultDeleter);
		this->chunk_size = duckdb_data_chunk_get_size(chunk.get());
		vectors.resize(columns.size());
		validities.resize(columns.size());
		for (idx_t col_idx = 0; col_idx < columns.size(); col_idx++) {
			auto vec = duckdb_data_chunk_get_vector(chunk.get(), col_idx);
			vectors[col_idx] = vec;
			validities[col_idx] = duckdb_vector_get_validity(vec);
		}
	}
};

struct LocalInitData {
	ExecState state = ExecState::UNINITIALIZED;
	std::unique_ptr<FileReader> reader;
	std::string insert_query;
	std::unique_ptr<QueryContext> ctx;
	std::vector<SQLSMALLINT> param_types;

	LocalInitData() {
	}

	static void Destroy(void *ldata_in) noexcept {
		auto ldata = reinterpret_cast<LocalInitData *>(ldata_in);
		delete ldata;
	}
};

} // namespace

static void Bind(duckdb_bind_info info) {
	auto conn_id_val = ValuePtr(duckdb_bind_get_parameter(info, 0), ValueDeleter);
	if (duckdb_is_null_value(conn_id_val.get())) {
		throw ScannerException("'odbc_copy_from' error: specified ODBC connection must be not NULL");
	}
	int64_t conn_id = duckdb_get_int64(conn_id_val.get());
	auto conn_ptr = ConnectionsRegistry::Remove(conn_id);

	if (conn_ptr.get() == nullptr) {
		throw ScannerException("'odbc_copy_from' error: ODBC connection not found on bind, id: " +
		                       std::to_string(conn_id));
	}

	// Return connection to registry
	ConnectionsRegistry::Add(std::move(conn_ptr));

	auto table_name_val = ValuePtr(duckdb_bind_get_parameter(info, 1), ValueDeleter);
	if (duckdb_is_null_value(table_name_val.get())) {
		throw ScannerException("'odbc_copy_from' error: specified table name must be not NULL");
	}
	auto table_name_ptr = VarcharPtr(duckdb_get_varchar(table_name_val.get()), VarcharDeleter);
	std::string table_name(table_name_ptr.get());

	auto file_path_val = ValuePtr(duckdb_bind_get_parameter(info, 2), ValueDeleter);
	if (duckdb_is_null_value(file_path_val.get())) {
		throw ScannerException("'odbc_copy_from' error: specified file path must be not NULL");
	}
	auto file_path_ptr = VarcharPtr(duckdb_get_varchar(file_path_val.get()), VarcharDeleter);
	std::string file_path(file_path_ptr.get());

	auto bdata_ptr = std::unique_ptr<BindData>(new BindData(conn_id, std::move(table_name), std::move(file_path)));
	duckdb_bind_set_bind_data(info, bdata_ptr.release(), BindData::Destroy);

	auto bool_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_BOOLEAN), LogicalTypeDeleter);
	auto ubigint_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_UBIGINT), LogicalTypeDeleter);
	auto double_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_DOUBLE), LogicalTypeDeleter);
	auto varchar_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_VARCHAR), LogicalTypeDeleter);

	duckdb_bind_add_result_column(info, "completed", bool_type.get());
	duckdb_bind_add_result_column(info, "records_inserted", ubigint_type.get());
	duckdb_bind_add_result_column(info, "records_per_second", double_type.get());
	duckdb_bind_add_result_column(info, "table_ddl", varchar_type.get());
}

static void GlobalInit(duckdb_init_info info) {
	BindData &bdata = *reinterpret_cast<BindData *>(duckdb_init_get_bind_data(info));
	// Keep the connection in global data while the function is running
	// to not allow other threads operate on it or close it.
	auto conn_ptr = ConnectionsRegistry::Remove(bdata.conn_id);
	auto gdata_ptr = std::unique_ptr<GlobalInitData>(new GlobalInitData(bdata.conn_id, std::move(conn_ptr)));
	duckdb_init_set_init_data(info, gdata_ptr.release(), GlobalInitData::Destroy);
}

static void LocalInit(duckdb_init_info info) {
	auto ldata_ptr = std::unique_ptr<LocalInitData>(new LocalInitData());
	duckdb_init_set_init_data(info, ldata_ptr.release(), LocalInitData::Destroy);
}

static void CheckSuccess(duckdb_state st, const std::string &msg) {
	if (st != DuckDBSuccess) {
		throw ScannerException("'odbc_copy_from' error: " + msg);
	}
}

static std::unique_ptr<FileReader> OpenFile(std::string &file_path) {
	duckdb_config config_bare = nullptr;
	duckdb_state state_config_create = duckdb_create_config(&config_bare);
	CheckSuccess(state_config_create, "'duckdb_create_config' failed");
	ConfigPtr config(config_bare, ConfigDeleter);
	duckdb_state state_config_threads = duckdb_set_config(config.get(), "threads", "1");
	CheckSuccess(state_config_threads, "'duckdb_set_config' threads=1 failed");

	duckdb_database db_bare = nullptr;
	duckdb_state state_db = duckdb_open_ext(NULL, &db_bare, config.get(), nullptr);
	CheckSuccess(state_db, "'duckdb_open_ext' failed");
	DatabasePtr db(db_bare, DatabaseDeleter);

	duckdb_connection conn_bare = nullptr;
	duckdb_state state_conn = duckdb_connect(db.get(), &conn_bare);
	CheckSuccess(state_conn, "'duckdb_connect' failed");
	ConnectionPtr conn(conn_bare, ConnectionDeleter);

	// todo: escaping, injections check, error message
	std::string select_query = "SELECT * FROM '" + file_path + "'";
	duckdb_result result;
	std::memset(&result, '\0', sizeof(result));
	duckdb_state state_select = duckdb_query(conn.get(), select_query.c_str(), &result);
	CheckSuccess(state_select, "error reading file, path: '" + file_path + "'");
	ResultPtr res_ptr(&result, ResultDeleter);

	DataChunkPtr chunk = DataChunkPtr(duckdb_fetch_chunk(result), DataChunkDeleter);
	CheckSuccess(chunk.get() != nullptr ? DuckDBSuccess : DuckDBError, "'duckdb_fetch_chunk' failed");
	idx_t col_count = duckdb_data_chunk_get_column_count(chunk.get());

	std::vector<FileColumn> columns;
	columns.reserve(col_count);
	for (idx_t col_idx = 0; col_idx < col_count; col_idx++) {
		const char *name_ptr = duckdb_column_name(&result, col_idx);
		CheckSuccess(name_ptr != nullptr ? DuckDBSuccess : DuckDBError,
		             "'duckdb_column_name' failed, column index: " + std::to_string(col_idx));
		std::string name(name_ptr);
		duckdb_type dtype = duckdb_column_type(&result, col_idx);
		FileColumn col(std::move(name), dtype);
		columns.emplace_back(std::move(col));
	}

	res_ptr.release();
	return std_make_unique<FileReader>(std::move(db), std::move(conn), result, std::move(chunk), std::move(columns));
}

static bool ReadRow(QueryContext &ctx, FileReader &reader, std::vector<ScannerValue> &row) {
	if (reader.row_idx >= reader.chunk_size) {
		duckdb_data_chunk chunk = duckdb_fetch_chunk(reader.result);
		if (0 == reader.chunk_size) {
			return false;
		}
		reader.chunk.reset(chunk);
		reader.chunk_size = duckdb_data_chunk_get_size(reader.chunk.get());
		if (0 == reader.chunk_size) {
			return false;
		}
		for (idx_t col_idx = 0; col_idx < row.size(); col_idx++) {
			auto vec = duckdb_data_chunk_get_vector(reader.chunk.get(), col_idx);
			reader.vectors[col_idx] = vec;
			reader.validities[col_idx] = duckdb_vector_get_validity(vec);
		}
		reader.row_idx = 0;
	}

	for (idx_t col_idx = 0; col_idx < row.size(); col_idx++) {
		duckdb_vector vec = reader.vectors.at(col_idx);
		uint64_t *validity = reader.validities.at(col_idx);
		// todo: faster validity check
		if (validity == nullptr || duckdb_validity_row_is_valid(validity, reader.row_idx)) {
			FileColumn &fc = reader.columns.at(col_idx);
			row[col_idx] = Types::ExtractNotNullParam(ctx.quirks, fc.dtype, vec, reader.row_idx, col_idx);
		} else {
			row[col_idx] = ScannerValue();
		}
	}
	reader.row_idx++;

	return true;
}

static std::string CreateInsertQuery(const std::string &table_name, const std::vector<FileColumn> &columns) {
	std::string query = "INSERT INTO ";
	query.append("\"");
	query.append(table_name);
	query.append("\"");
	query.append("\n(");
	for (size_t i = 0; i < columns.size(); i++) {
		const FileColumn &col = columns.at(i);
		query.append("\"");
		query.append(col.name);
		query.append("\"");
		if (i < columns.size() - 1) {
			query.append(",");
		}
		query.append("\n");
	}
	query.append(") VALUES (");
	for (size_t i = 0; i < columns.size(); i++) {
		query.append("?");
		if (i < columns.size() - 1) {
			query.append(",");
		}
	}
	query.append(")");

	return query;
}

static void CopyFrom(duckdb_function_info info, duckdb_data_chunk output) {
	BindData &bdata = *reinterpret_cast<BindData *>(duckdb_function_get_bind_data(info));
	GlobalInitData &gdata = *reinterpret_cast<GlobalInitData *>(duckdb_function_get_init_data(info));
	LocalInitData &ldata = *reinterpret_cast<LocalInitData *>(duckdb_function_get_local_init_data(info));

	if (ldata.state == ExecState::EXHAUSTED) {
		duckdb_data_chunk_set_size(output, 0);
		return;
	}

	if (ldata.state == ExecState::UNINITIALIZED) {
		OdbcConnection &conn = *gdata.conn_ptr;
		ldata.reader = OpenFile(bdata.file_path);
		FileReader &reader = *ldata.reader;
		ldata.insert_query = CreateInsertQuery(bdata.table_name, reader.columns);

		HSTMT hstmt = SQL_NULL_HSTMT;
		{
			SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, conn.dbc, &hstmt);
			if (!SQL_SUCCEEDED(ret)) {
				throw ScannerException("'SQLAllocHandle' failed for STMT handle, return: " + std::to_string(ret));
			}
		}
		{
			auto wquery = WideChar::Widen(ldata.insert_query.data(), ldata.insert_query.length());
			SQLRETURN ret = SQLPrepareW(hstmt, wquery.data(), wquery.length<SQLINTEGER>());
			if (!SQL_SUCCEEDED(ret)) {
				std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
				throw ScannerException("'SQLPrepare' failed, query: '" + ldata.insert_query +
				                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
			}
		}

		DbmsQuirks quirks(conn, std::map<std::string, ValuePtr>());
		ldata.ctx = std_make_unique<QueryContext>(ldata.insert_query, hstmt, quirks);
		ldata.param_types = Params::CollectTypes(*ldata.ctx);

		ldata.state = ExecState::EXECUTED;
	}

	QueryContext &ctx = *ldata.ctx;
	FileReader &reader = *ldata.reader;

	std::vector<ScannerValue> row;
	row.resize(reader.columns.size());

	while (ReadRow(ctx, reader, row)) {

		Params::SetExpectedTypes(ctx, ldata.param_types, row);
		Params::BindToOdbc(ctx, row);

		if (ctx.quirks.reset_stmt_before_execute) {
			SQLRETURN ret = SQLFreeStmt(ctx.hstmt, SQL_CLOSE);
			if (!SQL_SUCCEEDED(ret)) {
				std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
				throw ScannerException("'SQLFreeStmt' with SQL_CLOSE (reset_stmt_before_execute) failed, query: '" +
				                       ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag +
				                       "'");
			}
		}

		{
			SQLRETURN ret = SQLExecute(ctx.hstmt);
			if (!SQL_SUCCEEDED(ret)) {
				std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
				throw ScannerException("'SQLExecute' failed, query: '" + ctx.query +
				                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
			}
		}
	}

	duckdb_data_chunk_set_size(output, 0);
}

void OdbcCopyFromFunction::Register(duckdb_connection conn) {
	auto fun = TableFunctionPtr(duckdb_create_table_function(), TableFunctionDeleter);
	duckdb_table_function_set_name(fun.get(), "odbc_copy_from");

	// parameters
	auto varchar_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_VARCHAR), LogicalTypeDeleter);
	auto bigint_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_BIGINT), LogicalTypeDeleter);
	auto uint_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_UINTEGER), LogicalTypeDeleter);
	duckdb_table_function_add_parameter(fun.get(), bigint_type.get());  // connection handle
	duckdb_table_function_add_parameter(fun.get(), varchar_type.get()); // destination table name
	duckdb_table_function_add_parameter(fun.get(), varchar_type.get()); // parquet file path
	// named args
	duckdb_table_function_add_named_parameter(fun.get(), "batch_size", uint_type.get());
	// duckdb_table_function_add_named_parameter(fun.get(), "column_type_mapping", map_type.get());

	// callbacks
	duckdb_table_function_set_bind(fun.get(), odbc_copy_from_bind);
	duckdb_table_function_set_init(fun.get(), odbc_copy_from_init);
	duckdb_table_function_set_local_init(fun.get(), odbc_copy_from_local_init);
	duckdb_table_function_set_function(fun.get(), odbc_copy_from_function);

	// register and cleanup
	duckdb_state state = duckdb_register_table_function(conn, fun.get());

	if (state != DuckDBSuccess) {
		throw ScannerException("'odbc_copy_from' function registration failed");
	}
}

} // namespace odbcscanner

static void odbc_copy_from_bind(duckdb_bind_info info) noexcept {
	try {
		odbcscanner::Bind(info);
	} catch (std::exception &e) {
		duckdb_bind_set_error(info, e.what());
	}
}

static void odbc_copy_from_init(duckdb_init_info info) noexcept {
	try {
		odbcscanner::GlobalInit(info);
	} catch (std::exception &e) {
		duckdb_init_set_error(info, e.what());
	}
}

static void odbc_copy_from_local_init(duckdb_init_info info) noexcept {
	try {
		odbcscanner::LocalInit(info);
	} catch (std::exception &e) {
		duckdb_init_set_error(info, e.what());
	}
}

static void odbc_copy_from_function(duckdb_function_info info, duckdb_data_chunk output) noexcept {
	try {
		odbcscanner::CopyFrom(info, output);
	} catch (std::exception &e) {
		duckdb_function_set_error(info, e.what());
	}
}
