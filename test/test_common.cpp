#define CATCH_CONFIG_MAIN
#include "test_common.hpp"

#include <cstring>

#define SCANNER_QUOTE(value)                 #value
#define SCANNER_STR(value)                   SCANNER_QUOTE(value)
#define ODBC_SCANNER_EXTENSION_FILE_PATH_STR SCANNER_STR(ODBC_SCANNER_EXTENSION_FILE_PATH)

ScannerConn::ScannerConn() {
	duckdb_config config = nullptr;

	duckdb_state state_config_create = duckdb_create_config(&config);
	REQUIRE(state_config_create == DuckDBSuccess);

	duckdb_state state_config_set_1 = duckdb_set_config(config, "allow_unsigned_extensions", "true");
	REQUIRE(state_config_set_1 == DuckDBSuccess);
	duckdb_state state_config_set_2 = duckdb_set_config(config, "threads", "1");
	REQUIRE(state_config_set_2 == DuckDBSuccess);

	duckdb_state state_db = duckdb_open_ext(NULL, &db, config, nullptr);
	REQUIRE(state_db == DuckDBSuccess);

	duckdb_destroy_config(&config);

	duckdb_state state_conn = duckdb_connect(db, &conn);
	REQUIRE(state_conn == DuckDBSuccess);

	std::string load_query = "LOAD '" + std::string(ODBC_SCANNER_EXTENSION_FILE_PATH_STR) + "'";
	duckdb_state state_load = duckdb_query(conn, load_query.c_str(), nullptr);
	REQUIRE(state_load == DuckDBSuccess);

	duckdb_state state_odbc_conn =
	    duckdb_query(conn, "SET VARIABLE conn = odbc_connect('Driver={DuckDB Driver};threads=1;')", nullptr);
	REQUIRE(state_odbc_conn == DuckDBSuccess);
}

ScannerConn::~ScannerConn() {
	duckdb_state state_close = duckdb_query(conn, "SELECT odbc_close(getvariable('conn'))", nullptr);
	REQUIRE(state_close == DuckDBSuccess);

	duckdb_disconnect(&conn);
	duckdb_close(&db);
}

Result::~Result() noexcept {
	if (chunk != nullptr) {
		duckdb_destroy_data_chunk(&chunk);
	}
	duckdb_destroy_result(&res);
}

duckdb_result *Result::Get() {
	return &res;
}

bool Result::NextChunk() {
	bool first = chunk == nullptr;
	if (!first) {
		duckdb_destroy_data_chunk(&chunk);
	}
	this->chunk = duckdb_fetch_chunk(res);
	if (chunk == nullptr) {
		return false;
	}
	if (!first) {
		cur_row_idx += duckdb_vector_size();
	}
	return true;
}

template <typename CTYPE>
static CTYPE *NotNullData(duckdb_type expected_type_id, duckdb_data_chunk chunk, idx_t cur_row_idx, idx_t col_idx,
                          idx_t row_idx) {
	REQUIRE(chunk != nullptr);

	idx_t rel_row_idx = row_idx - cur_row_idx;
	REQUIRE(rel_row_idx < duckdb_vector_size());

	idx_t col_count = duckdb_data_chunk_get_column_count(chunk);
	REQUIRE(col_idx < col_count);

	duckdb_vector vec = duckdb_data_chunk_get_vector(chunk, col_idx);
	REQUIRE(vec != nullptr);

	auto ltype = LogicalTypePtr(duckdb_vector_get_column_type(vec), LogicalTypeDeleter);
	duckdb_type type_id = duckdb_get_type_id(ltype.get());
	REQUIRE(type_id == expected_type_id);

	uint64_t *validity = duckdb_vector_get_validity(vec);
	if (validity != nullptr) {
		REQUIRE(duckdb_validity_row_is_valid(validity, rel_row_idx));
	}

	return reinterpret_cast<CTYPE *>(duckdb_vector_get_data(vec));
}

template <>
uint8_t Result::Value<uint8_t>(idx_t col_idx, idx_t row_idx) {
	uint8_t *data = NotNullData<uint8_t>(DUCKDB_TYPE_UTINYINT, chunk, cur_row_idx, col_idx, row_idx);
	return data[row_idx];
}

template <>
int16_t Result::Value<int16_t>(idx_t col_idx, idx_t row_idx) {
	int16_t *data = NotNullData<int16_t>(DUCKDB_TYPE_SMALLINT, chunk, cur_row_idx, col_idx, row_idx);
	return data[row_idx];
}

template <>
int32_t Result::Value<int32_t>(idx_t col_idx, idx_t row_idx) {
	int32_t *data = NotNullData<int32_t>(DUCKDB_TYPE_INTEGER, chunk, cur_row_idx, col_idx, row_idx);
	return data[row_idx];
}

template <>
int64_t Result::Value<int64_t>(idx_t col_idx, idx_t row_idx) {
	int64_t *data = NotNullData<int64_t>(DUCKDB_TYPE_BIGINT, chunk, cur_row_idx, col_idx, row_idx);
	return data[row_idx];
}

template <>
std::string Result::Value<std::string>(idx_t col_idx, idx_t row_idx) {
	duckdb_string_t *data = NotNullData<duckdb_string_t>(DUCKDB_TYPE_VARCHAR, chunk, cur_row_idx, col_idx, row_idx);
	duckdb_string_t dstr = data[row_idx];
	const char *cstr = duckdb_string_t_data(&dstr);
	uint32_t len = duckdb_string_t_length(dstr);
	return std::string(cstr, len);
}
