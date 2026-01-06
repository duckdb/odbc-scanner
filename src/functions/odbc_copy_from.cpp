#include "odbc_scanner.hpp"

#include <cstring>
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

#include "capi_pointers.hpp"
#include "dbms_quirks.hpp"
#include "defer.hpp"
#include "diagnostics.hpp"
#include "duckdb_extension_api.hpp"
#include "make_unique.hpp"
#include "mappings.hpp"
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
	std::vector<std::string> source_queries;
	DbmsQuirks quirks;
	std::unordered_map<duckdb_type, std::string> mapping;
	bool create_table = false;
	uint64_t batch_size = 0;

	BindData(int64_t conn_id_in, std::string table_name_in, std::vector<std::string> source_queries_in,
	         DbmsQuirks quirks_in, std::unordered_map<duckdb_type, std::string> mapping_in, bool create_table_in,
	         uint64_t batch_size_in)
	    : conn_id(conn_id_in), table_name(std::move(table_name_in)), source_queries(std::move(source_queries_in)),
	      quirks(quirks_in), mapping(std::move(mapping_in)), create_table(create_table_in), batch_size(batch_size_in) {
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
	duckdb_type type_id;

	FileColumn();

	FileColumn(std::string name_in, duckdb_type type_id_in) : name(std::move(name_in)), type_id(type_id_in) {
	}
};

struct FileReader {
	DatabasePtr db;
	ConnectionPtr conn;
	ResultPtr result;
	DataChunkPtr chunk;
	idx_t chunk_size;
	idx_t row_idx = 0;
	std::vector<FileColumn> columns;
	std::vector<duckdb_vector> vectors;
	std::vector<uint64_t *> validities;

	FileReader(DatabasePtr db_in, ConnectionPtr conn_in, ResultPtr result_in, DataChunkPtr chunk_in,
	           std::vector<FileColumn> columns_in)
	    : db(std::move(db_in)), conn(std::move(conn_in)), result(std::move(result_in)), chunk(std::move(chunk_in)),
	      columns(std::move(columns_in)) {
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
	std::unique_ptr<QueryContext> ctx;
	std::vector<SQLSMALLINT> param_types;
	std::string create_table_query;
	uint64_t records_inserted = 0;
	uint64_t copy_start_moment = 0;
	uint64_t batch_start_moment = 0;

	LocalInitData() {
	}

	static void Destroy(void *ldata_in) noexcept {
		auto ldata = reinterpret_cast<LocalInitData *>(ldata_in);
		delete ldata;
	}
};

} // namespace

static std::vector<std::string> ExtractSourceQueries(duckdb_value file_path_val, duckdb_value source_query_val,
                                                     duckdb_value source_queries_val) {
	bool file_specified = file_path_val != nullptr && !duckdb_is_null_value(file_path_val);
	bool single_query_specified = source_query_val != nullptr && !duckdb_is_null_value(source_query_val);
	bool query_list_specified = source_queries_val != nullptr && !duckdb_is_null_value(source_queries_val);
	if (file_specified) {
		if (single_query_specified || query_list_specified) {
			throw ScannerException("'odbc_copy_from' error: only a single one from the following options can be "
			                       "specified: 'file_path', 'source_query', 'source_queries'");
		}
		auto file_path_ptr = VarcharPtr(duckdb_get_varchar(file_path_val), VarcharDeleter);
		std::string file_path_raw(file_path_ptr.get());
		std::string file_path_trimmed = Strings::Trim(file_path_raw);
		if (file_path_trimmed.empty()) {
			throw ScannerException("'odbc_copy_from' error: specified file path must be not empty");
		}
		std::string file_path;
		for (char ch : file_path_trimmed) {
			file_path.push_back(ch);
			if (ch == '\'') {
				file_path.push_back('\'');
			}
		}
		std::string query = "SELECT * FROM '" + file_path + "'";
		std::vector<std::string> res;
		res.emplace_back(std::move(query));
		return res;

	} else if (single_query_specified) {
		if (query_list_specified) {
			throw ScannerException("'odbc_copy_from' error: only a single one from the following options can be "
			                       "specified: 'file_path', 'source_query', 'source_queries'");
		}
		auto query_ptr = VarcharPtr(duckdb_get_varchar(source_query_val), VarcharDeleter);
		std::string query_raw(query_ptr.get());
		std::string query = Strings::Trim(query_raw);
		if (query.empty()) {
			throw ScannerException("'odbc_copy_from' error: specified source query must be not empty");
		}
		std::vector<std::string> res;
		res.emplace_back(std::move(query));
		return res;

	} else {
		idx_t count = duckdb_get_list_size(source_queries_val);
		if (0 == count) {
			throw ScannerException("'odbc_copy_from' error: source queries list must be not empty");
		}
		std::vector<std::string> res;
		res.reserve(count);
		for (idx_t i = 0; i < count; i++) {
			auto query_val = ValuePtr(duckdb_get_list_child(source_queries_val, i), ValueDeleter);
			auto query_ptr = VarcharPtr(duckdb_get_varchar(query_val.get()), VarcharDeleter);
			std::string query_raw(query_ptr.get());
			std::string query = Strings::Trim(query_raw);
			if (query.empty()) {
				throw ScannerException("'odbc_copy_from' error: specified source query must be not empty, index: " +
				                       std::to_string(i));
			}
			res.emplace_back(std::move(query));
		}
		return res;
	}
}

static std::unordered_map<duckdb_type, std::string> ResolveTypeMapping(duckdb_value column_types_val,
                                                                       OdbcConnection &conn, const DbmsQuirks &quirks) {
	if (column_types_val == nullptr) {
		return Mappings::Resolve(conn.driver, quirks);
	}

	if (!duckdb_is_null_value(column_types_val)) {
		throw ScannerException("'odbc_copy_from' error: specified 'column_types' map must be not NULL");
	}

	idx_t mapping_len = duckdb_get_map_size(column_types_val);
	if (mapping_len == 0) {
		throw ScannerException("'odbc_copy_from' error: specified 'column_types' map must be non empty");
	}

	std::unordered_map<duckdb_type, std::string> mapping;
	mapping.reserve(mapping_len);
	for (idx_t i = 0; i < mapping_len; i++) {
		auto key_val = ValuePtr(duckdb_get_map_key(column_types_val, i), ValueDeleter);
		if (key_val.get() == nullptr || duckdb_is_null_value(key_val.get())) {
			throw ScannerException("'odbc_copy_from' error: 'column_types' map keys must be not NULL, index: " +
			                       std::to_string(i));
		}
		auto key_cstr = VarcharPtr(duckdb_get_varchar(key_val.get()), VarcharDeleter);
		if (key_cstr.get() == nullptr) {
			throw ScannerException("'odbc_copy_from' error: 'column_types' map keys chars must be not NULL, index: " +
			                       std::to_string(i));
		}
		std::string key_raw(key_cstr.get());
		std::string key_str = Strings::Trim(key_raw);
		if (key_str.empty()) {
			throw ScannerException("'odbc_copy_from' error: 'column_types' map keys must be not blank, index: " +
			                       std::to_string(i));
		}
		duckdb_type key = Types::FromString(key_str);

		auto value_val = ValuePtr(duckdb_get_map_value(column_types_val, i), ValueDeleter);
		if (value_val.get() == nullptr || duckdb_is_null_value(value_val.get())) {
			throw ScannerException("'odbc_copy_from' error: 'column_types' map values must be not NULL, index: " +
			                       std::to_string(i));
		}
		auto value_cstr = VarcharPtr(duckdb_get_varchar(value_val.get()), VarcharDeleter);
		if (value_cstr.get() == nullptr) {
			throw ScannerException("'odbc_copy_from' error: 'column_types' map values chars must be not NULL, index: " +
			                       std::to_string(i));
		}
		std::string value_raw(value_cstr.get());
		std::string value = Strings::Trim(value_raw);
		if (value.empty()) {
			throw ScannerException("'odbc_copy_from' error: 'column_types' map values must be not blank, index: " +
			                       std::to_string(i));
		}

		mapping.emplace(std::move(key), std::move(value));
	}

	return mapping;
}

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
	// Return the connection to registry at the end of the block
	auto deferred = Defer([&conn_ptr] { ConnectionsRegistry::Add(std::move(conn_ptr)); });

	auto table_name_val = ValuePtr(duckdb_bind_get_parameter(info, 1), ValueDeleter);
	if (duckdb_is_null_value(table_name_val.get())) {
		throw ScannerException("'odbc_copy_from' error: specified table name must be not NULL");
	}
	auto table_name_ptr = VarcharPtr(duckdb_get_varchar(table_name_val.get()), VarcharDeleter);
	std::string table_name(table_name_ptr.get());

	auto file_path_val = ValuePtr(duckdb_bind_get_named_parameter(info, "file_path"), ValueDeleter);
	auto source_query_val = ValuePtr(duckdb_bind_get_named_parameter(info, "source_query"), ValueDeleter);
	auto source_queries_val = ValuePtr(duckdb_bind_get_named_parameter(info, "source_queries"), ValueDeleter);
	std::vector<std::string> source_queries =
	    ExtractSourceQueries(file_path_val.get(), source_query_val.get(), source_queries_val.get());

	OdbcConnection &conn = *conn_ptr;
	DbmsQuirks quirks(conn, std::map<std::string, ValuePtr>());

	auto column_types_val = ValuePtr(duckdb_bind_get_named_parameter(info, "column_types"), ValueDeleter);
	std::unordered_map<duckdb_type, std::string> mapping = ResolveTypeMapping(column_types_val.get(), conn, quirks);

	auto create_table_val = ValuePtr(duckdb_bind_get_named_parameter(info, "create_table"), ValueDeleter);
	bool create_table = create_table_val.get() != nullptr && !duckdb_is_null_value(create_table_val.get()) &&
	                    duckdb_get_bool(create_table_val.get());

	auto batch_size_val = ValuePtr(duckdb_bind_get_named_parameter(info, "batch_size"), ValueDeleter);
	uint64_t batch_size = duckdb_vector_size();
	if (batch_size_val.get() != nullptr && !duckdb_is_null_value(batch_size_val.get())) {
		batch_size = duckdb_get_uint64(batch_size_val.get());
	}

	auto bdata_ptr = std_make_unique<BindData>(conn_id, std::move(table_name), std::move(source_queries), quirks,
	                                           std::move(mapping), create_table, batch_size);
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
	auto gdata_ptr = std_make_unique<GlobalInitData>(bdata.conn_id, std::move(conn_ptr));
	duckdb_init_set_init_data(info, gdata_ptr.release(), GlobalInitData::Destroy);
}

static void LocalInit(duckdb_init_info info) {
	auto ldata_ptr = std_make_unique<LocalInitData>();
	duckdb_init_set_init_data(info, ldata_ptr.release(), LocalInitData::Destroy);
}

static void CheckSuccess(duckdb_state st, const std::string &msg) {
	if (st != DuckDBSuccess) {
		throw ScannerException("'odbc_copy_from' error: " + msg);
	}
}

static std::unique_ptr<FileReader> OpenReader(const std::vector<std::string> &source_queries) {
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

	if (source_queries.size() == 0) {
		throw ScannerException("'odbc_copy_from' error: no source queries specified");
	}
	for (size_t i = 0; i < source_queries.size() - 1; i++) {
		ResultPtr res(new duckdb_result(), ResultDeleter);
		const std::string &query = source_queries.at(i);
		duckdb_state state_query = duckdb_query(conn.get(), query.c_str(), res.get());
		if (state_query != DuckDBSuccess) {
			const char *cerr = duckdb_result_error(res.get());
			std::string err = cerr != nullptr ? std::string(cerr) : "N/A";
			throw ScannerException("'odbc_copy_from' error: source prep query failure, index: " + std::to_string(i) +
			                       ", sql: '" + query + "', message: '" + err + "'");
		}
	}
	const std::string &select_query = source_queries.at(source_queries.size() - 1);
	ResultPtr result(new duckdb_result(), ResultDeleter);
	duckdb_state state_select = duckdb_query(conn.get(), select_query.c_str(), result.get());
	if (state_select != DuckDBSuccess) {
		const char *cerr = duckdb_result_error(result.get());
		std::string err = cerr != nullptr ? std::string(cerr) : "N/A";
		throw ScannerException("'odbc_copy_from' error: source query failure, sql: '" + select_query + "', message: '" +
		                       err + "'");
	}

	DataChunkPtr chunk = DataChunkPtr(duckdb_fetch_chunk(*result), DataChunkDeleter);
	CheckSuccess(chunk.get() != nullptr ? DuckDBSuccess : DuckDBError, "'duckdb_fetch_chunk' failed");
	idx_t col_count = duckdb_data_chunk_get_column_count(chunk.get());

	std::vector<FileColumn> columns;
	columns.reserve(col_count);
	for (idx_t col_idx = 0; col_idx < col_count; col_idx++) {
		const char *name_ptr = duckdb_column_name(result.get(), col_idx);
		CheckSuccess(name_ptr != nullptr ? DuckDBSuccess : DuckDBError,
		             "'duckdb_column_name' failed, column index: " + std::to_string(col_idx));
		std::string name(name_ptr);
		duckdb_type type_id = duckdb_column_type(result.get(), col_idx);
		FileColumn col(std::move(name), type_id);
		columns.emplace_back(std::move(col));
	}

	return std_make_unique<FileReader>(std::move(db), std::move(conn), std::move(result), std::move(chunk),
	                                   std::move(columns));
}

static bool ReadRow(QueryContext &ctx, FileReader &reader, std::vector<ScannerValue> &row) {
	if (reader.row_idx >= reader.chunk_size) {
		duckdb_data_chunk chunk = duckdb_fetch_chunk(*reader.result);
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
			row[col_idx] = Types::ExtractNotNullParam(ctx.quirks, fc.type_id, vec, reader.row_idx, col_idx);
		} else {
			row[col_idx] = ScannerValue();
		}
	}
	reader.row_idx++;

	return true;
}

static const std::string &LookupMapping(std::unordered_map<duckdb_type, std::string> &mapping, duckdb_type type_id) {
	auto it = mapping.find(type_id);
	if (it == mapping.end()) {
		std::string dtype_name = Types::ToString(type_id);
		throw ScannerException("'odbc_copy_from' error: column type not recognized, id: " + std::to_string(type_id) +
		                       ", name: '" + dtype_name + "'");
	}
	return it->second;
}

static std::string BuildCreateTableQuery(const std::string &table_name, const std::vector<FileColumn> &columns,
                                         std::unordered_map<duckdb_type, std::string> &mapping) {
	std::string query = "CREATE TABLE ";
	query.append("\"");
	query.append(table_name);
	query.append("\"");
	query.append("(\n");
	for (size_t i = 0; i < columns.size(); i++) {
		const FileColumn &col = columns.at(i);
		query.append("\"");
		query.append(col.name);
		query.append("\"");
		query.append(" ");
		const std::string &type_name = LookupMapping(mapping, col.type_id);
		query.append(type_name);
		if (i < columns.size() - 1) {
			query.append(",");
		}
		query.append("\n");
	}
	query.append(")");
	return query;
}

static std::string BuildInsertQuery(const std::string &table_name, const std::vector<FileColumn> &columns) {
	std::string query = "INSERT INTO ";
	query.append("\"");
	query.append(table_name);
	query.append("\"");
	query.append("(\n");
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

static uint64_t CurrentTimeMillis() {
	auto time = std::chrono::system_clock::now();
	auto since_epoch = time.time_since_epoch();
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch);
	return static_cast<uint64_t>(millis.count());
}

static void SetResultsRow(duckdb_data_chunk output, bool completed, BindData &bdata, LocalInitData &ldata) {
	duckdb_vector completed_vec = duckdb_data_chunk_get_vector(output, 0);
	duckdb_vector records_inserted_vec = duckdb_data_chunk_get_vector(output, 1);
	duckdb_vector records_per_second_vec = duckdb_data_chunk_get_vector(output, 2);
	duckdb_vector table_ddl_vec = duckdb_data_chunk_get_vector(output, 3);
	if (completed_vec == nullptr || records_inserted_vec == nullptr || records_per_second_vec == nullptr ||
	    table_ddl_vec == nullptr) {
		throw ScannerException("Invalid NULL output vector");
	}
	bool *completed_data = reinterpret_cast<bool *>(duckdb_vector_get_data(completed_vec));
	uint64_t *records_inserted_data = reinterpret_cast<uint64_t *>(duckdb_vector_get_data(records_inserted_vec));
	double *records_per_second_data = reinterpret_cast<double *>(duckdb_vector_get_data(records_per_second_vec));
	if (completed_data == nullptr || records_inserted_data == nullptr || records_per_second_data == nullptr) {
		throw ScannerException("Invalid NULL output vector datareceived");
	}

	completed_data[0] = completed;
	records_inserted_data[0] = ldata.records_inserted;
	double now = static_cast<double>(CurrentTimeMillis());
	if (completed) {
		records_per_second_data[0] =
		    static_cast<double>(ldata.records_inserted) / (now - static_cast<double>(ldata.copy_start_moment)) * 1000;
		if (bdata.create_table) {
			duckdb_vector_assign_string_element_len(table_ddl_vec, 0, ldata.create_table_query.c_str(),
			                                        ldata.create_table_query.length());
		} else {
			duckdb_vector_ensure_validity_writable(table_ddl_vec);
			uint64_t *table_ddl_validity = duckdb_vector_get_validity(table_ddl_vec);
			duckdb_validity_set_row_invalid(table_ddl_validity, 0);
		}
	} else {
		records_per_second_data[0] =
		    static_cast<double>(bdata.batch_size) / (now - static_cast<double>(ldata.batch_start_moment)) * 1000;
		duckdb_vector_ensure_validity_writable(table_ddl_vec);
		uint64_t *table_ddl_validity = duckdb_vector_get_validity(table_ddl_vec);
		duckdb_validity_set_row_invalid(table_ddl_validity, 0);
	}
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
		ldata.reader = OpenReader(bdata.source_queries);
		FileReader &reader = *ldata.reader;

		HSTMT hstmt = SQL_NULL_HSTMT;
		{
			SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, conn.dbc, &hstmt);
			if (!SQL_SUCCEEDED(ret)) {
				throw ScannerException("'SQLAllocHandle' failed for STMT handle, return: " + std::to_string(ret));
			}
		}

		if (bdata.create_table) {
			std::string query = BuildCreateTableQuery(bdata.table_name, reader.columns, bdata.mapping);
			auto wquery = WideChar::Widen(query.data(), query.length());
			SQLRETURN ret = SQLExecDirectW(hstmt, wquery.data(), wquery.length<SQLINTEGER>());
			if (!SQL_SUCCEEDED(ret)) {
				std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
				throw ScannerException("'SQLExecDirectW' failed, query: '" + query +
				                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
			}
			ldata.create_table_query = query;
		}

		std::string insert_query = BuildInsertQuery(bdata.table_name, reader.columns);
		{
			auto wquery = WideChar::Widen(insert_query.data(), insert_query.length());
			SQLRETURN ret = SQLPrepareW(hstmt, wquery.data(), wquery.length<SQLINTEGER>());
			if (!SQL_SUCCEEDED(ret)) {
				std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
				throw ScannerException("'SQLPrepare' failed, query: '" + insert_query +
				                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
			}
		}

		ldata.ctx = std_make_unique<QueryContext>(insert_query, hstmt, bdata.quirks);
		QueryContext &ctx = *ldata.ctx;
		ldata.param_types = Params::CollectTypes(ctx);
		ldata.copy_start_moment = CurrentTimeMillis();
		ldata.batch_start_moment = ldata.copy_start_moment;

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

		ldata.records_inserted++;
		if (bdata.batch_size > 0 && ldata.records_inserted % bdata.batch_size == 0) {
			SetResultsRow(output, false, bdata, ldata);
			ldata.batch_start_moment = CurrentTimeMillis();
			duckdb_data_chunk_set_size(output, 1);
			return;
		}
	}

	ldata.state = ExecState::EXHAUSTED;
	SetResultsRow(output, true, bdata, ldata);
	duckdb_data_chunk_set_size(output, 1);
}

void OdbcCopyFromFunction::Register(duckdb_connection conn) {
	auto fun = TableFunctionPtr(duckdb_create_table_function(), TableFunctionDeleter);
	duckdb_table_function_set_name(fun.get(), "odbc_copy_from");

	// parameters
	auto varchar_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_VARCHAR), LogicalTypeDeleter);
	auto bigint_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_BIGINT), LogicalTypeDeleter);
	auto uint_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_UINTEGER), LogicalTypeDeleter);
	auto bool_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_BOOLEAN), LogicalTypeDeleter);
	auto map_type = LogicalTypePtr(duckdb_create_map_type(varchar_type.get(), varchar_type.get()), LogicalTypeDeleter);
	auto list_type = LogicalTypePtr(duckdb_create_list_type(varchar_type.get()), LogicalTypeDeleter);
	duckdb_table_function_add_parameter(fun.get(), bigint_type.get());  // connection handle
	duckdb_table_function_add_parameter(fun.get(), varchar_type.get()); // destination table name
	// named args
	duckdb_table_function_add_named_parameter(fun.get(), "file_path", varchar_type.get());
	duckdb_table_function_add_named_parameter(fun.get(), "source_query", varchar_type.get());
	duckdb_table_function_add_named_parameter(fun.get(), "source_queries", list_type.get());
	duckdb_table_function_add_named_parameter(fun.get(), "batch_size", uint_type.get());
	duckdb_table_function_add_named_parameter(fun.get(), "create_table", bool_type.get());
	duckdb_table_function_add_named_parameter(fun.get(), "column_types", map_type.get());

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
