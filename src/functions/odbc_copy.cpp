#include "odbc_scanner.hpp"

#include <cstring>
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

#include "function_args.hpp"
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

static void odbc_copy_bind(duckdb::cxx::TableFunction::BindInput &input);
static void odbc_copy_init(duckdb::cxx::TableFunction::InitGlobalInput &input);
static void odbc_copy_local_init(duckdb::cxx::TableFunction::InitLocalInput &input);
static void odbc_copy_function(duckdb::cxx::TableFunction::ExecInput &input);

namespace odbcscanner {

namespace {

enum class ExecState { UNINITIALIZED, EXECUTED, EXHAUSTED };

struct ReaderOptions {
	std::string conn_string;
	std::vector<std::string> queries;
	uint64_t limit;

	ReaderOptions(std::string conn_string_in, std::vector<std::string> queries_in, uint64_t limit_in)
	    : conn_string(std::move(conn_string_in)), queries(std::move(queries_in)), limit(limit_in) {
	}
};

struct InsertOptions {
	static const uint32_t default_batch_size = 16;

	uint32_t batch_size = 0;
	bool use_insert_all = false;
	bool use_insert_union = false;
	std::string dummy_table_name;
	bool copy_in_transaction = false;
	uint64_t max_records_in_transaction = 0;
	std::string dest_table;
	std::string dest_query;
	std::string dest_query_single;

	InsertOptions(uint32_t batch_size_in, bool use_insert_all_in, bool use_insert_union_in,
	              std::string dummy_table_name_in, bool copy_in_transaction_in, uint64_t max_records_in_transaction_in,
	              std::string dest_table_in, std::string dest_query_in, std::string dest_query_single_in)
	    : batch_size(batch_size_in), use_insert_all(use_insert_all_in), use_insert_union(use_insert_union_in),
	      dummy_table_name(std::move(dummy_table_name_in)), copy_in_transaction(copy_in_transaction_in),
	      max_records_in_transaction(max_records_in_transaction_in), dest_table(std::move(dest_table_in)),
	      dest_query(std::move(dest_query_in)), dest_query_single(std::move(dest_query_single_in)) {
	}
};

struct CreateTableOptions {
	bool do_create_table = false;
	std::unordered_map<LogicalTypeId, std::string> column_types;
	std::string column_quotes;
	bool commit_after = false;

	CreateTableOptions(bool do_create_table_in, std::unordered_map<LogicalTypeId, std::string> column_types_in,
	                   std::string column_quotes_in, bool commit_after_in)
	    : do_create_table(do_create_table_in), column_types(column_types_in),
	      column_quotes(std::move(column_quotes_in)), commit_after(commit_after_in) {
	}
};

struct GeneralOptions {
	bool close_connection = false;

	GeneralOptions(bool close_connection_in) : close_connection(close_connection_in) {
	}
};

struct BindData {
	int64_t conn_id = 0;
	DbmsQuirks quirks;
	ReaderOptions reader_options;
	InsertOptions insert_options;
	CreateTableOptions create_table_options;
	GeneralOptions general_options;

	BindData(int64_t conn_id_in, DbmsQuirks quirks_in, ReaderOptions reader_options_in, InsertOptions insert_options_in,
	         CreateTableOptions create_table_options_in, GeneralOptions general_options_in)
	    : conn_id(conn_id_in), quirks(quirks_in), reader_options(std::move(reader_options_in)),
	      insert_options(insert_options_in), create_table_options(std::move(create_table_options_in)),
	      general_options(general_options_in) {
	}
};

struct GlobalInitData {
	int64_t conn_id;
	std::unique_ptr<OdbcConnection> conn_ptr;
	bool close_connection = false;

	GlobalInitData(int64_t conn_id, std::unique_ptr<OdbcConnection> conn_ptr_in, bool close_connection_in)
	    : conn_id(conn_id), conn_ptr(std::move(conn_ptr_in)), close_connection(close_connection_in) {
		if (!conn_ptr) {
			throw ScannerException("'odbc_copy' error: ODBC connection not found on global init, id: " +
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
};

struct SourceColumn {
	std::string name;
	LogicalTypeId type_id;
	std::vector<uint32_t> typmods;

	SourceColumn();

	SourceColumn(std::string name_in, LogicalTypeId type_id_in, std::vector<uint32_t> typmods_in)
	    : name(std::move(name_in)), type_id(type_id_in), typmods(std::move(typmods_in)) {
	}
};

// Executes all statements from the specified SQL string, all results except
// the last one are drained, result of the last statement is returned.
static std::unique_ptr<QueryResult> ExecuteQuery(Connection &conn, const std::string &query) {
	auto iter = conn.ParseSQL(query);
	std::unique_ptr<QueryResult> res;
	for (;;) {
		SqlStatement stmt = iter.Next();
		if (!stmt) {
			break;
		}
		if (res) {
			// not the last statement
			res->Drain();
			res.reset();
		}
		res.reset(new QueryResult(conn.Execute(stmt)));
	}
	if (!res) {
		throw ScannerException("No statements to execute");
	}
	return res;
}

struct SourceReader {
	Environment env;
	Instance db;
	Connection conn;
	std::string select_query;
	uint64_t limit;

	std::string current_query;
	std::unique_ptr<QueryResult> result;
	std::unique_ptr<DataChunk> chunk;
	idx_t chunk_size = 0;
	idx_t chunks_count = 0;
	idx_t row_idx = 0;
	std::vector<SourceColumn> columns;
	std::vector<Vector> vectors;
	std::vector<VectorView> views;
	uint64_t offset = 0;
	bool exhausted = false;

	SourceReader(Environment env_in, Instance db_in, Connection conn_in, std::string select_query_in, uint64_t limit_in)
	    : env(std::move(env_in)), db(std::move(db_in)), conn(std::move(conn_in)),
	      select_query(std::move(select_query_in)), limit(limit_in) {

		this->NextChunkInternal();
		idx_t col_count = chunk ? chunk->GetVectorCount() : 0;
		this->columns.reserve(col_count);
		if (col_count > 0) {
			Schema schema = result->GetSchema();
			for (idx_t col_idx = 0; col_idx < col_count; col_idx++) {
				std::string name(schema.GetFieldName(col_idx));
				LogicalType ltype = schema.GetFieldType(col_idx);
				LogicalTypeId type_id = Types::TypeIdOf(ltype);
				std::vector<uint32_t> typmods;
				if (type_id == LogicalTypeId::DECIMAL) {
					uint8_t width = ltype.GetDecimalWidth();
					typmods.push_back(static_cast<uint32_t>(width));
					uint8_t scale = ltype.GetDecimalScale();
					typmods.push_back(static_cast<uint32_t>(scale));
				}
				SourceColumn col(std::move(name), type_id, std::move(typmods));
				this->columns.emplace_back(std::move(col));
			}
		}
		this->ResetVectorsInternal();
	}

	std::unique_ptr<QueryResult> ExecuteSelect() {
		std::string query = select_query;
		if (limit > 0) {
			query.append("\n");
			query.append("LIMIT ");
			query.append(std::to_string(limit));
			query.append(" OFFSET ");
			query.append(std::to_string(offset));
		}

		std::unique_ptr<QueryResult> res;
		try {
			res = ExecuteQuery(conn, query);
		} catch (std::exception &e) {
			throw ScannerException("'odbc_copy' error: source query failure, sql: '" + query + "', message: '" +
			                       std::string(e.what()) + "'");
		}
		this->current_query = query;

		offset += limit;
		return res;
	}

	std::unique_ptr<DataChunk> FetchChunk() {
		try {
			DataChunk fetched = result->FetchChunk();
			if (!fetched) {
				return std::unique_ptr<DataChunk>();
			}
			return std::unique_ptr<DataChunk>(new DataChunk(std::move(fetched)));
		} catch (std::exception &e) {
			throw ScannerException("'odbc_copy' error: source query failure, sql: '" + current_query + "', message: '" +
			                       std::string(e.what()) + "'");
		}
	}

	void NextChunkInternal() {
		if (exhausted) {
			return;
		}

		// initial query
		if (!result && !chunk) {
			this->result = ExecuteSelect();
		}

		// get next chunk from current result
		this->chunk = FetchChunk();

		if (!chunk) { // result exhausted

			if (limit == 0 || chunk_size * chunks_count < limit) { // we are on the last result
				this->exhausted = true;
				return;
			}

			// lets check if we have next limited result
			this->result.reset();
			this->result = ExecuteSelect();
			this->chunks_count = 0;
			this->chunk = FetchChunk();
			if (!chunk) {
				this->exhausted = true;
				return;
			}
		}

		this->chunks_count += 1;
		this->chunk_size = chunk->GetRowCount();
	}

	void ResetVectorsInternal() {
		vectors.clear();
		views.clear();
		for (idx_t col_idx = 0; col_idx < columns.size(); col_idx++) {
			Vector vec = chunk->GetVector(col_idx);
			// source data is read by row index
			vec.Flatten();
			views.push_back(vec.GetView());
			vectors.emplace_back(std::move(vec));
		}
	}

	bool NextChunk() {
		this->NextChunkInternal();
		if (!chunk || chunk_size == 0) {
			return false;
		}
		this->ResetVectorsInternal();
		this->row_idx = 0;
		return true;
	}

	bool ReadRow(const QueryContext &ctx, std::vector<ScannerValue> &row) {
		if (row_idx >= chunk_size) {
			return false;
		}
		for (idx_t col_idx = 0; col_idx < row.size(); col_idx++) {
			Vector &vec = vectors.at(col_idx);
			const VectorView &view = views.at(col_idx);
			// todo: faster validity check
			if (view.RowIsValid(row_idx)) {
				SourceColumn &fc = columns.at(col_idx);
				row[col_idx] = Types::ExtractNotNullParam(ctx.quirks, fc.type_id, vec, row_idx, col_idx);
			} else {
				row[col_idx] = ScannerValue();
			}
		}
		this->row_idx++;
		return true;
	}
};

struct LocalInitData {
	ExecState state = ExecState::UNINITIALIZED;
	std::unique_ptr<SourceReader> reader;
	std::unique_ptr<QueryContext> ctx;
	std::vector<SQLSMALLINT> param_types;
	std::string create_table_query;
	uint64_t records_inserted = 0;
	uint64_t copy_start_moment = 0;
	uint64_t chunk_start_moment = 0;
	uint32_t last_prepared_batch_size = 0;
	SQLUINTEGER orig_transaction_mode = SQL_AUTOCOMMIT_DEFAULT;
	uint64_t inserted_in_transaction = 0;
	// single_row_buf and flat_batch are held on the LocalInitData so each
	// ScannerValue slot keeps a stable address across CopyInTransaction calls —
	// that is what makes it safe for the bind caches to remember a
	// SQLBindParameter pointer from a previous execute.
	std::vector<ScannerValue> single_row_buf;
	std::vector<ScannerValue> flat_batch;
	BindCache single_row_bind_cache;
	BindCache batch_bind_cache;

	LocalInitData() {
	}
};

} // namespace

// Data chunk is created with the default capacity that is equal to the engine vector size
static idx_t StandardVectorSize(Factory &factory) {
	std::vector<LogicalType> types;
	types.emplace_back(factory.CreateType(LogicalTypeId::BOOLEAN));
	return DataChunk(factory, types).GetCapacity();
}

static ReaderOptions ExtractReaderOptions(const Value *source_conn_string_val, const Value *source_file_val,
                                          const Value *source_query_val, const Value *source_queries_val,
                                          const Value *source_limit_val) {
	std::string source_conn_string = ":memory:";
	if (source_conn_string_val != nullptr && !source_conn_string_val->IsNull()) {
		std::string source_conn_string_raw = std::string(source_conn_string_val->Get<varchar_t>().view());
		source_conn_string = Strings::Trim(source_conn_string_raw);
		if (source_conn_string.empty()) {
			throw ScannerException("'odbc_copy' error: specified DuckDB connection string must be not empty");
		}
	}

	bool file_specified = source_file_val != nullptr && !source_file_val->IsNull();
	bool single_query_specified = source_query_val != nullptr && !source_query_val->IsNull();
	bool query_list_specified = source_queries_val != nullptr && !source_queries_val->IsNull();
	std::vector<std::string> queries;
	if (file_specified) {
		if (single_query_specified || query_list_specified) {
			throw ScannerException("'odbc_copy' error: only a single one from the following options can be "
			                       "specified: 'source_file', 'source_query', 'source_queries'");
		}
		std::string source_file_raw = std::string(source_file_val->Get<varchar_t>().view());
		std::string source_file_trimmed = Strings::Trim(source_file_raw);
		if (source_file_trimmed.empty()) {
			throw ScannerException("'odbc_copy' error: specified file path must be not empty");
		}
		std::string source_file;
		for (char ch : source_file_trimmed) {
			source_file.push_back(ch);
			if (ch == '\'') {
				source_file.push_back('\'');
			}
		}
		std::string query = "SELECT * FROM '" + source_file + "'";
		queries.emplace_back(std::move(query));

	} else if (single_query_specified) {
		if (query_list_specified) {
			throw ScannerException("'odbc_copy' error: only a single one from the following options can be "
			                       "specified: 'source_file', 'source_query', 'source_queries'");
		}
		std::string query_raw = std::string(source_query_val->Get<varchar_t>().view());
		std::string query = Strings::Trim(query_raw);
		if (query.empty()) {
			throw ScannerException("'odbc_copy' error: specified source query must be not empty");
		}
		queries.emplace_back(std::move(query));

	} else {
		idx_t count = source_queries_val != nullptr ? source_queries_val->GetChildCount() : 0;
		if (0 == count) {
			throw ScannerException("'odbc_copy' error: source queries list must be not empty");
		}
		queries.reserve(count);
		for (idx_t i = 0; i < count; i++) {
			auto query_val = std::unique_ptr<Value>(new Value(source_queries_val->GetChild(i)));
			std::string query_raw = std::string(query_val->Get<varchar_t>().view());
			std::string query = Strings::Trim(query_raw);
			if (query.empty()) {
				throw ScannerException("'odbc_copy' error: specified source query must be not empty, index: " +
				                       std::to_string(i));
			}
			queries.emplace_back(std::move(query));
		}
	}

	uint64_t source_limit = 0;
	if (source_limit_val != nullptr && !source_limit_val->IsNull()) {
		source_limit = source_limit_val->Get<uint64_t>();
		if (source_limit < 2048 || source_limit % 2048 != 0) {
			throw ScannerException(
			    "'odbc_copy' error: invalid source limit specified: " + std::to_string(source_limit) +
			    ", must be larger or equal to 2048 and be dividable by 2048 without a remainder");
		}
	}

	return ReaderOptions(std::move(source_conn_string), std::move(queries), source_limit);
}

static std::unordered_map<LogicalTypeId, std::string> ExtractTypeMapping(const Value *column_types_val) {
	if (column_types_val == nullptr) {
		return std::unordered_map<LogicalTypeId, std::string>();
	}

	if (column_types_val->IsNull()) {
		throw ScannerException("'odbc_copy' error: specified 'column_types' map must be not NULL");
	}

	// map children alternate keys and values
	idx_t mapping_len = column_types_val->GetChildCount() / 2;
	if (mapping_len == 0) {
		throw ScannerException("'odbc_copy' error: specified 'column_types' map must be non empty");
	}

	std::unordered_map<LogicalTypeId, std::string> mapping;
	mapping.reserve(mapping_len);
	for (idx_t i = 0; i < mapping_len; i++) {
		auto key_val = std::unique_ptr<Value>(new Value(column_types_val->GetChild(i * 2)));
		if (key_val.get() == nullptr || key_val->IsNull()) {
			throw ScannerException("'odbc_copy' error: 'column_types' map keys must be not NULL, index: " +
			                       std::to_string(i));
		}
		std::string key_raw = std::string(key_val->Get<varchar_t>().view());
		std::string key_str = Strings::Trim(key_raw);
		if (key_str.empty()) {
			throw ScannerException("'odbc_copy' error: 'column_types' map keys must be not blank, index: " +
			                       std::to_string(i));
		}
		LogicalTypeId key = Types::FromString(key_str);

		auto value_val = std::unique_ptr<Value>(new Value(column_types_val->GetChild(i * 2 + 1)));
		if (value_val.get() == nullptr || value_val->IsNull()) {
			throw ScannerException("'odbc_copy' error: 'column_types' map values must be not NULL, index: " +
			                       std::to_string(i));
		}
		std::string value_raw = std::string(value_val->Get<varchar_t>().view());
		std::string value = Strings::Trim(value_raw);
		if (value.empty()) {
			throw ScannerException("'odbc_copy' error: 'column_types' map values must be not blank, index: " +
			                       std::to_string(i));
		}

		mapping.emplace(key, std::move(value));
	}

	return mapping;
}

static InsertOptions ExtractInsertOptions(DbmsDriver driver, idx_t engine_vector_size, const Value *batch_size_val,
                                          const Value *use_insert_all_val, const Value *use_insert_union_val,
                                          const Value *dummy_table_name_val, const Value *copy_in_transaction_val,
                                          const Value *max_records_in_transaction_val, const Value *dest_table_val,
                                          const Value *dest_query_val, const Value *dest_query_single_val) {
	uint32_t batch_size = InsertOptions::default_batch_size;
	if (batch_size_val != nullptr && !batch_size_val->IsNull()) {
		batch_size = batch_size_val->Get<uint32_t>();
	}
	if (batch_size > engine_vector_size || engine_vector_size % batch_size != 0) {
		std::string msg;
		if (engine_vector_size == 2048) { // default size
			msg = "supported values: 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048";
		} else {
			msg = "must be a divisor of the engine vector size: " + std::to_string(engine_vector_size);
		}
		throw ScannerException("'odbc_copy' error: invalid value specified for the 'batch_size' parameter: " +
		                       std::to_string(batch_size) + ", " + msg);
	}

	bool use_insert_all = driver == DbmsDriver::ORACLE;
	if (use_insert_all_val != nullptr && !use_insert_all_val->IsNull()) {
		use_insert_all = use_insert_all_val->Get<bool>();
	}

	bool use_insert_union = driver == DbmsDriver::FIREBIRD;
	if (use_insert_union_val != nullptr && !use_insert_union_val->IsNull()) {
		use_insert_union = use_insert_union_val->Get<bool>();
	}

	std::string dummy_table_name;
	if (driver == DbmsDriver::ORACLE) {
		dummy_table_name = "dual";
	} else if (driver == DbmsDriver::DB2) {
		dummy_table_name = "sysibm.sysdummy1";
	} else if (driver == DbmsDriver::FIREBIRD) {
		dummy_table_name = "rdb$database";
	}
	if (dummy_table_name_val != nullptr && !dummy_table_name_val->IsNull()) {
		dummy_table_name = std::string(dummy_table_name_val->Get<varchar_t>().view());
	}

	bool copy_in_transaction = true;
	if (copy_in_transaction_val != nullptr && !copy_in_transaction_val->IsNull()) {
		copy_in_transaction = copy_in_transaction_val->Get<bool>();
	}

	uint64_t max_records_in_transaction = 0;
	if (max_records_in_transaction_val != nullptr && !max_records_in_transaction_val->IsNull()) {
		max_records_in_transaction = max_records_in_transaction_val->Get<uint64_t>();
	}

	std::string dest_table;
	if (dest_table_val != nullptr && !dest_table_val->IsNull()) {
		dest_table = std::string(dest_table_val->Get<varchar_t>().view());
	}

	std::string dest_query;
	if (dest_query_val != nullptr && !dest_query_val->IsNull()) {
		dest_query = std::string(dest_query_val->Get<varchar_t>().view());
	}

	if (dest_table.empty() && dest_query.empty()) {
		throw ScannerException("'odbc_copy' error: either 'dest_table' or `dest_query` option must be specicifed");
	}

	if (!dest_table.empty() && !dest_query.empty()) {
		throw ScannerException(
		    "'odbc_copy' error: 'dest_table' and `dest_query` options cannot be both specified in the same call");
	}

	std::string dest_query_single;
	if (dest_query_single_val != nullptr && !dest_query_single_val->IsNull()) {
		dest_query_single = std::string(dest_query_single_val->Get<varchar_t>().view());
	}

	return InsertOptions(batch_size, use_insert_all, use_insert_union, std::move(dummy_table_name), copy_in_transaction,
	                     max_records_in_transaction, std::move(dest_table), std::move(dest_query),
	                     std::move(dest_query_single));
}

static CreateTableOptions ExtractCreateTableOptions(const InsertOptions &insert_options, DbmsDriver driver,
                                                    DbmsQuirks &quirks, const Value *create_table_val,
                                                    const Value *column_types_val, const Value *column_quotes_val,
                                                    const Value *commit_after_create_table_val) {
	bool create_table = false;
	if (create_table_val != nullptr && !create_table_val->IsNull()) {
		create_table = create_table_val->Get<bool>();
	}

	if (create_table && insert_options.dest_table.empty()) {
		throw ScannerException("'odbc_copy' error: when 'create_table=TRUE' the 'dest_table' option must be specified");
	}

	std::unordered_map<LogicalTypeId, std::string> column_types;
	column_types = Mappings::Resolve(driver, quirks);
	std::unordered_map<LogicalTypeId, std::string> column_types_user = ExtractTypeMapping(column_types_val);
	for (auto en : column_types_user) {
		column_types[en.first] = en.second;
	}

	std::string column_quotes = "\"";
	if (column_quotes_val != nullptr && !column_quotes_val->IsNull()) {
		column_quotes = std::string(column_quotes_val->Get<varchar_t>().view());
	}

	bool commit_after_create_table = driver == DbmsDriver::FIREBIRD;
	if (commit_after_create_table_val != nullptr && !commit_after_create_table_val->IsNull()) {
		commit_after_create_table = commit_after_create_table_val->Get<bool>();
	}

	return CreateTableOptions(create_table, std::move(column_types), std::move(column_quotes),
	                          commit_after_create_table);
}

static GeneralOptions ExtractGeneralOptions(const Value *close_connection_val, bool conn_must_be_closed) {
	bool close_connection = conn_must_be_closed;
	if (close_connection_val != nullptr && !close_connection_val->IsNull()) {
		close_connection = close_connection_val->Get<bool>();
		if (conn_must_be_closed && !close_connection) {
			throw ScannerException("'odbc_copy' error: 'close_connection=FALSE' option cannot be specified along with "
			                       "a connection string");
		}
	}
	return GeneralOptions(close_connection);
}

static void Bind(TableFunction::BindInput &input) {
	Factory &factory = input.GetContext().GetFactory();
	OdbcConnection::VerifyExternalAccess(input.GetContext());
	auto conn_id_or_str_val = FunctionArgs::GetArg(input, 0);
	auto extracted_conn = OdbcConnection::ExtractOrOpen("odbc_copy", conn_id_or_str_val.get());
	// Return the connection to registry at the end of the block
	auto deferred = Defer([&extracted_conn] { ConnectionsRegistry::Add(std::move(extracted_conn.ptr)); });

	auto source_conn_string_val = FunctionArgs::GetNamedArg(input, "source_conn_string");
	auto source_file_val = FunctionArgs::GetNamedArg(input, "source_file");
	auto source_query_val = FunctionArgs::GetNamedArg(input, "source_query");
	auto source_queries_val = FunctionArgs::GetNamedArg(input, "source_queries");
	auto source_limit_val = FunctionArgs::GetNamedArg(input, "source_limit");
	ReaderOptions reader_options =
	    ExtractReaderOptions(source_conn_string_val.get(), source_file_val.get(), source_query_val.get(),
	                         source_queries_val.get(), source_limit_val.get());

	OdbcConnection &conn = *extracted_conn.ptr;

	std::map<std::string, std::unique_ptr<Value>> user_quirks = DbmsQuirks::ExtractUserQuirks(input);
	DbmsQuirks quirks(conn, user_quirks);

	auto batch_size_val = FunctionArgs::GetNamedArg(input, "batch_size");
	auto use_insert_all_val = FunctionArgs::GetNamedArg(input, "use_insert_all");
	auto use_insert_union_val = FunctionArgs::GetNamedArg(input, "use_insert_union");
	auto dummy_table_name_val = FunctionArgs::GetNamedArg(input, "dummy_table_name");
	auto copy_in_transaction_val = FunctionArgs::GetNamedArg(input, "copy_in_transaction");
	auto max_records_in_transaction_val = FunctionArgs::GetNamedArg(input, "max_records_in_transaction");
	auto dest_table_val = FunctionArgs::GetNamedArg(input, "dest_table");
	auto dest_query_val = FunctionArgs::GetNamedArg(input, "dest_query");
	auto dest_query_single_val = FunctionArgs::GetNamedArg(input, "dest_query_single");
	idx_t engine_vector_size = StandardVectorSize(factory);
	InsertOptions insert_options = ExtractInsertOptions(
	    conn.driver, engine_vector_size, batch_size_val.get(), use_insert_all_val.get(), use_insert_union_val.get(),
	    dummy_table_name_val.get(), copy_in_transaction_val.get(), max_records_in_transaction_val.get(),
	    dest_table_val.get(), dest_query_val.get(), dest_query_single_val.get());

	auto create_table_val = FunctionArgs::GetNamedArg(input, "create_table");
	auto column_types_val = FunctionArgs::GetNamedArg(input, "column_types");
	auto column_quotes_val = FunctionArgs::GetNamedArg(input, "column_quotes");
	auto commit_after_create_table_val = FunctionArgs::GetNamedArg(input, "commit_after_create_table");
	CreateTableOptions create_table_options =
	    ExtractCreateTableOptions(insert_options, conn.driver, quirks, create_table_val.get(), column_types_val.get(),
	                              column_quotes_val.get(), commit_after_create_table_val.get());

	auto close_connection_val = FunctionArgs::GetNamedArg(input, "close_connection");
	GeneralOptions general_options = ExtractGeneralOptions(close_connection_val.get(), extracted_conn.must_be_closed);

	input.SetBindData<BindData>(extracted_conn.id, quirks, std::move(reader_options), insert_options,
	                            std::move(create_table_options), general_options);

	auto bool_type = factory.CreateType(LogicalTypeId::BOOLEAN);
	auto ubigint_type = factory.CreateType(LogicalTypeId::UBIGINT);
	auto float_type = factory.CreateType(LogicalTypeId::FLOAT);
	auto varchar_type = factory.CreateType(LogicalTypeId::VARCHAR);

	input.AddResultColumn("completed", bool_type);
	input.AddResultColumn("rows_processed", ubigint_type);
	input.AddResultColumn("elapsed_seconds", float_type);
	input.AddResultColumn("rows_per_second", float_type);
	input.AddResultColumn("table_ddl", varchar_type);
}

static void GlobalInit(TableFunction::InitGlobalInput &input) {
	const BindData &bdata = input.GetBindData<BindData>();
	// Keep the connection in global data while the function is running
	// to not allow other threads operate on it or close it.
	auto conn_ptr = ConnectionsRegistry::Remove(bdata.conn_id);
	input.SetGlobalState<GlobalInitData>(bdata.conn_id, std::move(conn_ptr), bdata.general_options.close_connection);
}

static void LocalInit(TableFunction::InitLocalInput &input) {
	input.SetLocalState<LocalInitData>();
}

static std::unique_ptr<SourceReader> OpenReader(const ReaderOptions &options) {
	Environment env;
	Instance db(env, options.conn_string);
	db.GetConfig().SetOption("threads", "1");
	Connection conn = db.Connect();

	if (options.queries.size() == 0) {
		throw ScannerException("'odbc_copy' error: no source queries specified");
	}
	for (size_t i = 0; i < options.queries.size() - 1; i++) {
		const std::string &query = options.queries.at(i);
		try {
			ExecuteQuery(conn, query)->Drain();
		} catch (std::exception &e) {
			throw ScannerException("'odbc_copy' error: source prep query failure, index: " + std::to_string(i) +
			                       ", sql: '" + query + "', message: '" + std::string(e.what()) + "'");
		}
	}
	const std::string &select_query = options.queries.at(options.queries.size() - 1);

	return std_make_unique<SourceReader>(std::move(env), std::move(db), std::move(conn), select_query, options.limit);
}

static std::string LookupMapping(const std::unordered_map<LogicalTypeId, std::string> &mapping,
                                 const SourceColumn &col) {
	auto it = mapping.find(col.type_id);
	if (it == mapping.end()) {
		std::string dtype_name = Types::ToString(col.type_id);
		throw ScannerException("'odbc_copy' error: column type not recognized, id: " +
		                       std::to_string(static_cast<uint32_t>(col.type_id)) + ", name: '" + dtype_name + "'");
	}
	std::string res = it->second;
	for (size_t i = 0; i < col.typmods.size(); i++) {
		uint32_t tm = col.typmods.at(i);
		std::string snippet = "{typmod" + std::to_string(i + 1) + "}";
		std::string replacement = std::to_string(tm);
		Strings::ReplaceAll(res, snippet, replacement);
	}
	return res;
}

static std::string BuildCreateTableQuery(const std::string &table_name, const std::vector<SourceColumn> &columns,
                                         const CreateTableOptions &options) {
	std::string query = "CREATE TABLE ";
	query.append(table_name);
	query.append(" (\n");
	for (size_t i = 0; i < columns.size(); i++) {
		const SourceColumn &col = columns.at(i);
		query.append("    ");
		query.append(options.column_quotes);
		query.append(col.name);
		query.append(options.column_quotes);
		query.append(" ");
		const std::string &type_name = LookupMapping(options.column_types, col);
		query.append(type_name);
		if (i < columns.size() - 1) {
			query.append(",");
		}
		query.append("\n");
	}
	query.append(")");
	return query;
}

static std::string BuildInsertQuery(const std::vector<SourceColumn> &columns, const InsertOptions &options,
                                    const CreateTableOptions &create_options, uint32_t batch_size) {
	// query explicitly specified by user, not need to generate one
	if (!options.dest_query.empty()) {
		if (batch_size == 1 && !options.dest_query_single.empty()) {
			return options.dest_query_single;
		}
		return options.dest_query;
	}

	std::string query = "INSERT ";

	if (options.use_insert_all && batch_size > 1) {
		query.append("ALL\n");
		for (uint32_t row_idx = 0; row_idx < batch_size; row_idx++) {
			query.append("INTO ");
			query.append(options.dest_table);
			query.append(" (");
			for (size_t i = 0; i < columns.size(); i++) {
				const SourceColumn &col = columns.at(i);
				query.append(create_options.column_quotes);
				query.append(col.name);
				query.append(create_options.column_quotes);
				if (i < columns.size() - 1) {
					query.append(",");
				}
			}
			query.append(") VALUES ");
			query.append("(");
			for (size_t col_idx = 0; col_idx < columns.size(); col_idx++) {
				query.append("?");
				if (col_idx < columns.size() - 1) {
					query.append(",");
				}
			}
			query.append(")\n");
		}
		query.append("SELECT 1");
		if (!options.dummy_table_name.empty()) {
			query.append(" FROM ");
			query.append(options.dummy_table_name);
		}

	} else if (options.use_insert_union && batch_size > 1) {
		query.append("INTO ");
		query.append(options.dest_table);
		query.append(" SELECT * FROM (\n");
		for (uint32_t row_idx = 0; row_idx < batch_size; row_idx++) {
			query.append("SELECT ");
			for (size_t i = 0; i < columns.size(); i++) {
				const SourceColumn &col = columns.at(i);
				query.append("CAST (? AS ");
				const std::string &type_name = LookupMapping(create_options.column_types, col);
				query.append(type_name);
				query.append(") AS ");
				query.append(create_options.column_quotes);
				query.append(col.name);
				query.append(create_options.column_quotes);
				if (i < columns.size() - 1) {
					query.append(",");
				}
				query.append(" ");
			}
			if (!options.dummy_table_name.empty()) {
				query.append("FROM ");
				query.append(options.dummy_table_name);
			}
			query.append("\n");
			if (row_idx < batch_size - 1) {
				query.append("UNION ALL\n");
			}
		}
		query.append(") AS a");

	} else {
		query.append("INTO ");
		query.append(options.dest_table);
		query.append(" (\n");
		for (size_t i = 0; i < columns.size(); i++) {
			const SourceColumn &col = columns.at(i);
			query.append(create_options.column_quotes);
			query.append(col.name);
			query.append(create_options.column_quotes);
			if (i < columns.size() - 1) {
				query.append(",");
			}
			query.append("\n");
		}
		query.append(") VALUES\n");

		for (uint32_t row_idx = 0; row_idx < batch_size; row_idx++) {
			query.append("(");
			for (size_t col_idx = 0; col_idx < columns.size(); col_idx++) {
				query.append("?");
				if (col_idx < columns.size() - 1) {
					query.append(",");
				}
			}
			query.append(")");
			if (row_idx < batch_size - 1) {
				query.append(",\n");
			}
		}
	}

	return query;
}

static uint64_t CurrentTimeMillis() {
	auto time = std::chrono::system_clock::now();
	auto since_epoch = time.time_since_epoch();
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch);
	return static_cast<uint64_t>(millis.count());
}

static void SetResultsRow(DataChunk &output, bool completed, const BindData &bdata, LocalInitData &ldata,
                          idx_t chunk_size) {
	Vector completed_vec = output.GetVector(0);
	Vector records_inserted_vec = output.GetVector(1);
	Vector elapsed_seconds_vec = output.GetVector(2);
	Vector records_per_second_vec = output.GetVector(3);
	Vector table_ddl_vec = output.GetVector(4);
	if (!completed_vec || !records_inserted_vec || !elapsed_seconds_vec || !records_per_second_vec || !table_ddl_vec) {
		throw ScannerException("Invalid NULL output vector");
	}
	bool *completed_data = completed_vec.GetDataMutable<bool>();
	uint64_t *records_inserted_data = records_inserted_vec.GetDataMutable<uint64_t>();
	float *elapsed_seconds_data = elapsed_seconds_vec.GetDataMutable<float>();
	float *records_per_second_data = records_per_second_vec.GetDataMutable<float>();
	if (completed_data == nullptr || records_inserted_data == nullptr || elapsed_seconds_data == nullptr ||
	    records_per_second_data == nullptr) {
		throw ScannerException("Invalid NULL output vector data received");
	}

	uint64_t now = CurrentTimeMillis();
	uint64_t elapsed_millis = now - ldata.copy_start_moment;
	completed_data[0] = completed;
	records_inserted_data[0] = ldata.records_inserted;
	elapsed_seconds_data[0] = static_cast<float>(static_cast<double>(elapsed_millis) / static_cast<double>(1000));
	if (completed) {
		records_per_second_data[0] =
		    static_cast<float>(static_cast<double>(ldata.records_inserted) / static_cast<double>(elapsed_millis) *
		                       static_cast<double>(1000));
		if (bdata.create_table_options.do_create_table) {
			table_ddl_vec.AssignStringUnsafe(
			    0, std::string_view(ldata.create_table_query.c_str(), ldata.create_table_query.length()));
		} else {
			Types::SetNullValueToResult(table_ddl_vec, 0);
		}
	} else {
		records_per_second_data[0] =
		    static_cast<float>(static_cast<double>(chunk_size) / static_cast<double>(now - ldata.chunk_start_moment) *
		                       static_cast<double>(1000));
		Types::SetNullValueToResult(table_ddl_vec, 0);
	}
}

static std::string PrepareInsert(HSTMT hstmt, const BindData &bdata, const std::vector<SourceColumn> columns,
                                 uint32_t batch_size) {
	std::string query = BuildInsertQuery(columns, bdata.insert_options, bdata.create_table_options, batch_size);
	auto wquery = WideChar::Widen(query.data(), query.length());
	SQLRETURN ret = SQLPrepareW(hstmt, wquery.data(), wquery.length<SQLINTEGER>());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLPrepare' failed, query: '" + query + "', return: " + std::to_string(ret) +
		                       ", diagnostics: '" + diag + "'");
	}
	return query;
}

static void SetTransactionMode(OdbcConnection &conn, SQLUINTEGER mode) {
	SQLRETURN ret =
	    SQLSetConnectAttr(conn.dbc, SQL_ATTR_AUTOCOMMIT, reinterpret_cast<SQLPOINTER>(static_cast<uint64_t>(mode)), 0);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(conn.dbc, SQL_HANDLE_DBC);
		throw ScannerException("'SQLSetConnectAttr' failed for SQL_ATTR_AUTOCOMMIT, return: " + std::to_string(ret) +
		                       ", diagnostics: '" + diag + "'");
	}
}

static SQLUINTEGER BeginTransaction(OdbcConnection &conn) {
	SQLUINTEGER mode = 0;
	SQLRETURN ret =
	    SQLGetConnectAttr(conn.dbc, SQL_ATTR_AUTOCOMMIT, reinterpret_cast<SQLPOINTER>(&mode), sizeof(mode), nullptr);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(conn.dbc, SQL_HANDLE_DBC);
		throw ScannerException("'SQLGetConnectAttr' failed for SQL_ATTR_AUTOCOMMIT, return: " + std::to_string(ret) +
		                       ", diagnostics: '" + diag + "'");
	}
	SetTransactionMode(conn, SQL_AUTOCOMMIT_OFF);
	return mode;
}

static void Commit(OdbcConnection &conn) {
	SQLRETURN ret = SQLEndTran(SQL_HANDLE_DBC, conn.dbc, SQL_COMMIT);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(conn.dbc, SQL_HANDLE_DBC);
		throw ScannerException("'SQLEndTran' failed for SQL_COMMIT, return: " + std::to_string(ret) +
		                       ", diagnostics: '" + diag + "'");
	}
}

static void Rollback(OdbcConnection &conn) {
	SQLRETURN ret = SQLEndTran(SQL_HANDLE_DBC, conn.dbc, SQL_ROLLBACK);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(conn.dbc, SQL_HANDLE_DBC);
		throw ScannerException("'SQLEndTran' failed for SQL_ROLLBACK, return: " + std::to_string(ret) +
		                       ", diagnostics: '" + diag + "'");
	}
}

static uint32_t BatchSizeForChunk(const InsertOptions &insert_options, const SourceReader &reader, size_t row_idx) {
	size_t avail = row_idx > 0 ? row_idx : reader.chunk_size;
	if (insert_options.batch_size <= avail) {
		return insert_options.batch_size;
	}
	if (!insert_options.dest_query.empty() && !insert_options.dest_query_single.empty()) {
		return static_cast<uint32_t>(1);
	}
	return static_cast<uint32_t>(avail);
}

static void CopyInTransaction(TableFunction::ExecInput &input, DataChunk &output) {
	const BindData &bdata = input.GetBindData<BindData>();
	GlobalInitData &gdata = input.GetGlobalState<GlobalInitData>();
	LocalInitData &ldata = input.GetLocalState<LocalInitData>();
	OdbcConnection &conn = *gdata.conn_ptr;
	size_t row_idx = 0;

	if (ldata.state == ExecState::UNINITIALIZED) {
		ldata.reader = OpenReader(bdata.reader_options);
		SourceReader &reader = *ldata.reader;

		StmtHandlePtr hstmt(nullptr, StmtHandleDeleter);
		{
			HSTMT hstmt_out = SQL_NULL_HSTMT;
			SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, conn.dbc, &hstmt_out);
			if (!SQL_SUCCEEDED(ret)) {
				throw ScannerException("'SQLAllocHandle' failed for STMT handle, return: " + std::to_string(ret));
			}
			hstmt.reset(hstmt_out);
		}

		if (bdata.create_table_options.do_create_table) {
			std::string query =
			    BuildCreateTableQuery(bdata.insert_options.dest_table, reader.columns, bdata.create_table_options);
			auto wquery = WideChar::Widen(query.data(), query.length());
			SQLRETURN ret = SQLExecDirectW(hstmt.get(), wquery.data(), wquery.length<SQLINTEGER>());
			if (!SQL_SUCCEEDED(ret)) {
				std::string diag = Diagnostics::Read(hstmt.get(), SQL_HANDLE_STMT);
				throw ScannerException("'SQLExecDirectW' failed, query: '" + query +
				                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
			}
			ldata.create_table_query = query;
			if (bdata.insert_options.copy_in_transaction && bdata.create_table_options.commit_after) {
				Commit(conn);
			}
		}

		uint32_t batch_size = BatchSizeForChunk(bdata.insert_options, reader, row_idx);
		std::string insert_query = PrepareInsert(hstmt.get(), bdata, reader.columns, batch_size);
		ldata.last_prepared_batch_size = batch_size;

		ldata.ctx = std_make_unique<QueryContext>(insert_query, std::move(hstmt), bdata.quirks);
		QueryContext &ctx = *ldata.ctx;
		ldata.param_types = Params::CollectTypes(ctx);
		ldata.copy_start_moment = CurrentTimeMillis();
		ldata.chunk_start_moment = ldata.copy_start_moment;

		ldata.state = ExecState::EXECUTED;
	}

	QueryContext &ctx = *ldata.ctx;
	SourceReader &reader = *ldata.reader;

	std::vector<ScannerValue> row;
	row.resize(reader.columns.size());
	size_t needed_flat_batch_size = ldata.last_prepared_batch_size * row.size();
	if (ldata.flat_batch.size() != needed_flat_batch_size) {
		// First call, batch-size transition, or column-count transition. Vector
		// may reallocate; any cached batch bindings now point at stale storage.
		ldata.flat_batch.clear();
		ldata.flat_batch.resize(needed_flat_batch_size);
		ldata.batch_bind_cache.Reset();
	}
	if (ldata.single_row_buf.size() != row.size()) {
		// Column-count transition (only relevant on the first call per reader) —
		// reinit the buffer and drop any stale bind cache.
		ldata.single_row_buf.clear();
		ldata.single_row_buf.resize(row.size());
		ldata.single_row_bind_cache.Reset();
	}
	std::vector<ScannerValue> &single_row_buf = ldata.single_row_buf;
	std::vector<ScannerValue> &flat_batch = ldata.flat_batch;

	ldata.chunk_start_moment = CurrentTimeMillis();
	for (;;) {
		bool has_row = reader.ReadRow(ctx, row);

		if (has_row) {
			for (size_t i = 0; i < row.size(); i++) {
				ScannerValue &val = row[i];
				size_t batch_idx = i + (row_idx * row.size());
				flat_batch[batch_idx] = std::move(val);
			}
			row_idx++;
			if (row_idx < ldata.last_prepared_batch_size) {
				continue;
			}
		}

		if (row_idx == 0) {
			break;
		}

		// at this point we have either full or incomplete flat batch

		bool tail_insert_prepared = false;
		bool use_batch_insert = true;
		size_t flat_batch_idx = 0;

		for (;;) {

			if (row_idx < ldata.last_prepared_batch_size && !tail_insert_prepared) {
				flat_batch.resize(row_idx * row.size());
				uint32_t batch_size = BatchSizeForChunk(bdata.insert_options, reader, row_idx);
				ctx.query = PrepareInsert(ctx.hstmt(), bdata, reader.columns, batch_size);
				ldata.param_types.resize(batch_size * row.size());
				ldata.last_prepared_batch_size = batch_size;
				tail_insert_prepared = true;
				use_batch_insert = batch_size > 1;
				// The prepared statement has been replaced — SQLPrepare invalidates
				// any existing bindings, so both caches must be dropped.
				ldata.single_row_bind_cache.Reset();
				ldata.batch_bind_cache.Reset();
			}

			if (use_batch_insert) {
				Params::SetExpectedTypes(ctx, ldata.param_types, flat_batch);
				Params::BindToOdbcWithCache(ctx, flat_batch, ldata.batch_bind_cache);
			} else {
				for (size_t i = 0; i < row.size(); i++) {
					single_row_buf[i] = std::move(flat_batch.at(flat_batch_idx + i));
				}
				flat_batch_idx += row.size();
				Params::SetExpectedTypes(ctx, ldata.param_types, single_row_buf);
				Params::BindToOdbcWithCache(ctx, single_row_buf, ldata.single_row_bind_cache);
			}

			// Always close any cursor / result-set state left by the previous
			// SQLExecute before the next one. Without this, some drivers (DuckDB
			// ODBC in particular) leave the statement in a state that makes a
			// later SQLEndTran fail with SQLSTATE HY010 "Function sequence
			// error". The previous implementation only did this on the
			// reset_stmt_before_execute quirk path or when cached bindings were
			// reused; doing it unconditionally simplifies the loop and matches
			// the spec — SQL_CLOSE on a statement with no open cursor is a
			// no-op.
			{
				SQLRETURN ret = SQLFreeStmt(ctx.hstmt(), SQL_CLOSE);
				if (!SQL_SUCCEEDED(ret)) {
					std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
					throw ScannerException("'SQLFreeStmt' with SQL_CLOSE failed, query: '" + ctx.query +
					                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
				}
			}

			{
				SQLRETURN ret = SQLExecute(ctx.hstmt());
				if (!SQL_SUCCEEDED(ret) && ret != SQL_NO_DATA) {
					std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
					throw ScannerException("'SQLExecute' failed, query: '" + ctx.query +
					                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
				}
			}

			if (use_batch_insert || flat_batch_idx >= flat_batch.size()) {
				break;
			}
		}

		ldata.records_inserted += row_idx;
		ldata.inserted_in_transaction += row_idx;
		row_idx = 0;

		if (bdata.insert_options.copy_in_transaction && bdata.insert_options.max_records_in_transaction > 0 &&
		    ldata.inserted_in_transaction > bdata.insert_options.max_records_in_transaction) {
			Commit(conn);
			ldata.inserted_in_transaction = 0;
		}

		if (!has_row) {
			break;
		}
	}

	idx_t prev_chunk_size = reader.chunk_size;
	bool has_next_chunk = reader.NextChunk();
	bool completed = !has_next_chunk;
	SetResultsRow(output, completed, bdata, ldata, prev_chunk_size);
	output.GetVector(0).SetSize(1);

	if (completed) {
		ldata.state = ExecState::EXHAUSTED;
	} else {
		uint32_t batch_size = BatchSizeForChunk(bdata.insert_options, reader, row_idx);
		if (batch_size != ldata.last_prepared_batch_size) {
			ctx.query = PrepareInsert(ctx.hstmt(), bdata, reader.columns, batch_size);
			ldata.param_types = Params::CollectTypes(ctx);
			ldata.last_prepared_batch_size = batch_size;
			// SQLPrepare invalidated the existing bindings on the statement
			// handle; both caches must be dropped before the next bind.
			ldata.single_row_bind_cache.Reset();
			ldata.batch_bind_cache.Reset();
		}
	}
}

static void Copy(TableFunction::ExecInput &input, DataChunk &output) {
	const BindData &bdata = input.GetBindData<BindData>();
	GlobalInitData &gdata = input.GetGlobalState<GlobalInitData>();
	LocalInitData &ldata = input.GetLocalState<LocalInitData>();

	OdbcConnection &conn = *gdata.conn_ptr;

	if (ldata.state == ExecState::EXHAUSTED) {
		if (bdata.insert_options.copy_in_transaction) {
			Commit(conn);
			SetTransactionMode(conn, ldata.orig_transaction_mode);
		}
		output.GetVector(0).SetSize(0);
		return;
	}

	if (bdata.insert_options.copy_in_transaction && ldata.state == ExecState::UNINITIALIZED) {
		ldata.orig_transaction_mode = BeginTransaction(conn);
	}

	try {
		CopyInTransaction(input, output);
	} catch (const std::exception &e) {
		std::string primary = e.what();
		if (bdata.insert_options.copy_in_transaction) {
			// Keep the primary CopyInTransaction error even when Rollback / restoring
			// the auto-commit mode throws on top. Without this, a Rollback-time
			// SQLSTATE HY010 could hide the actual root cause inside CopyInTransaction.
			try {
				Rollback(conn);
			} catch (const std::exception &re) {
				throw ScannerException(primary + " (also, Rollback failed: " + re.what() + ")");
			}
			try {
				SetTransactionMode(conn, ldata.orig_transaction_mode);
			} catch (const std::exception &se) {
				throw ScannerException(primary + " (also, SetTransactionMode failed: " + se.what() + ")");
			}
		}
		throw ScannerException(primary);
	}
}

void OdbcCopyFunction::Register(Extension &extension, Factory &factory) {
	auto fun = TableFunction::Create(factory);
	fun.SetName("odbc_copy");
	FunctionSignature sig = fun.GetSignature();

	// parameters
	auto varchar_type = factory.CreateType(LogicalTypeId::VARCHAR);
	auto bigint_type = factory.CreateType(LogicalTypeId::BIGINT);
	auto uint_type = factory.CreateType(LogicalTypeId::UINTEGER);
	auto ubigint_type = factory.CreateType(LogicalTypeId::UBIGINT);
	auto bool_type = factory.CreateType(LogicalTypeId::BOOLEAN);
	auto map_type = factory.ParseType("MAP(VARCHAR, VARCHAR)");
	auto list_type = factory.ParseType("VARCHAR[]");
	auto any_type = factory.CreateType(LogicalTypeId::ANY);
	sig.AddParameter("connection", any_type, FunctionParameterKind::POSITIONAL_ONLY); // connection handle or string
	// reader options
	FunctionArgs::AddNamedParameter(factory, sig, "source_conn_string", varchar_type);
	FunctionArgs::AddNamedParameter(factory, sig, "source_file", varchar_type);
	FunctionArgs::AddNamedParameter(factory, sig, "source_query", varchar_type);
	FunctionArgs::AddNamedParameter(factory, sig, "source_queries", list_type);
	FunctionArgs::AddNamedParameter(factory, sig, "source_limit", ubigint_type);
	// insert options
	FunctionArgs::AddNamedParameter(factory, sig, "batch_size", uint_type);
	FunctionArgs::AddNamedParameter(factory, sig, "use_insert_all", bool_type);
	FunctionArgs::AddNamedParameter(factory, sig, "use_insert_union", bool_type);
	FunctionArgs::AddNamedParameter(factory, sig, "dummy_table_name", varchar_type);
	FunctionArgs::AddNamedParameter(factory, sig, "copy_in_transaction", bool_type);
	FunctionArgs::AddNamedParameter(factory, sig, "max_records_in_transaction", ubigint_type);
	FunctionArgs::AddNamedParameter(factory, sig, "dest_table", varchar_type);
	FunctionArgs::AddNamedParameter(factory, sig, "dest_query", varchar_type);
	FunctionArgs::AddNamedParameter(factory, sig, "dest_query_single", varchar_type);
	// create table options
	FunctionArgs::AddNamedParameter(factory, sig, "create_table", bool_type);
	FunctionArgs::AddNamedParameter(factory, sig, "column_types", map_type);
	FunctionArgs::AddNamedParameter(factory, sig, "column_quotes", varchar_type);
	FunctionArgs::AddNamedParameter(factory, sig, "commit_after_create_table", bool_type);
	// general options
	FunctionArgs::AddNamedParameter(factory, sig, "close_connection", bool_type);
	// quirks
	FunctionArgs::AddNamedParameter(factory, sig, "decimal_params_as_chars", bool_type);
	FunctionArgs::AddNamedParameter(factory, sig, "integral_params_as_decimals", bool_type);

	// callbacks
	fun.SetBindCallback(odbc_copy_bind);
	fun.SetInitGlobalCallback(odbc_copy_init);
	fun.SetInitLocalCallback(odbc_copy_local_init);
	fun.SetExecCallback(odbc_copy_function);

	// register and cleanup
	try {
		extension.Register(fun);
	} catch (std::exception &e) {
		throw ScannerException(std::string("\'") + "odbc_copy" + "\' function registration failed: " + e.what());
	}
}

} // namespace odbcscanner

static void odbc_copy_bind(duckdb::cxx::TableFunction::BindInput &input) {
	try {
		odbcscanner::Bind(input);
	} catch (std::exception &e) {
		throw odbcscanner::FunctionArgs::BinderError(e.what());
	}
}

static void odbc_copy_init(duckdb::cxx::TableFunction::InitGlobalInput &input) {
	odbcscanner::GlobalInit(input);
}

static void odbc_copy_local_init(duckdb::cxx::TableFunction::InitLocalInput &input) {
	odbcscanner::LocalInit(input);
}

static void odbc_copy_function(duckdb::cxx::TableFunction::ExecInput &input) {
	duckdb::cxx::DataChunk output = input.GetOutputChunk();
	odbcscanner::Copy(input, output);
}
