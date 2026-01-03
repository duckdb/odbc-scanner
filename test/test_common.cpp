#define CATCH_CONFIG_MAIN
#include "test_common.hpp"

#include <cstring>

#define SCANNER_QUOTE(value)                 #value
#define SCANNER_STR(value)                   SCANNER_QUOTE(value)
#define ODBC_SCANNER_EXTENSION_FILE_PATH_STR SCANNER_STR(ODBC_SCANNER_EXTENSION_FILE_PATH)

ScannerConn::ScannerConn(bool establish_odbc_connection) {
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

	Result res_load;
	std::string load_query = "LOAD '" + std::string(ODBC_SCANNER_EXTENSION_FILE_PATH_STR) + "'";
	duckdb_state state_load = duckdb_query(conn, load_query.c_str(), res_load.Get());
	REQUIRE(QuerySuccess(res_load.Get(), state_load));

	if (!establish_odbc_connection) {
		return;
	}

	char *conn_cstr = std::getenv("ODBC_CONN_STRING");
	std::string conn_str = conn_cstr != nullptr ? std::string(conn_cstr) : "Driver={DuckDB Driver};threads=1;";

	std::string sql = std::string() + "SET VARIABLE conn = odbc_connect('" + conn_str + "')";
	Result conn_res;
	duckdb_state state_odbc_conn = duckdb_query(conn, sql.c_str(), conn_res.Get());
	if (DuckDBError == state_odbc_conn) {
		std::cerr << duckdb_result_error(conn_res.Get()) << std::endl;
	}
	REQUIRE(state_odbc_conn == DuckDBSuccess);
	this->odbc_connection_established = true;
}

ScannerConn::~ScannerConn() noexcept {
	if (odbc_connection_established) {
		duckdb_state state_close = duckdb_query(conn, "SELECT odbc_close(getvariable('conn'))", nullptr);
		REQUIRE(state_close == DuckDBSuccess);
	}

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

bool Result::IsNull(idx_t col_idx, idx_t row_idx) {
	REQUIRE(chunk != nullptr);

	idx_t rel_row_idx = row_idx - cur_row_idx;
	REQUIRE(rel_row_idx < duckdb_vector_size());

	idx_t col_count = duckdb_data_chunk_get_column_count(chunk);
	REQUIRE(col_idx < col_count);

	duckdb_vector vec = duckdb_data_chunk_get_vector(chunk, col_idx);
	REQUIRE(vec != nullptr);

	uint64_t *validity = duckdb_vector_get_validity(vec);
	if (validity != nullptr) {
		return !duckdb_validity_row_is_valid(validity, rel_row_idx);
	}

	return false;
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

static duckdb_type ColumnType(duckdb_data_chunk chunk, idx_t col_idx) {
	REQUIRE(chunk != nullptr);

	idx_t col_count = duckdb_data_chunk_get_column_count(chunk);
	REQUIRE(col_idx < col_count);

	duckdb_vector vec = duckdb_data_chunk_get_vector(chunk, col_idx);
	REQUIRE(vec != nullptr);

	auto ltype = LogicalTypePtr(duckdb_vector_get_column_type(vec), LogicalTypeDeleter);
	return duckdb_get_type_id(ltype.get());
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
	if (DBMSConfigured("MariaDB") && DUCKDB_TYPE_INTEGER == ColumnType(chunk, col_idx)) {
		int32_t *data = NotNullData<int32_t>(DUCKDB_TYPE_INTEGER, chunk, cur_row_idx, col_idx, row_idx);
		return data[row_idx];
	}
	if (DBMSConfigured("Oracle")) {
		int64_t *data = NotNullData<int64_t>(DUCKDB_TYPE_DECIMAL, chunk, cur_row_idx, col_idx, row_idx);
		return data[row_idx];
	}
	if (DBMSConfigured("Snowflake")) {
		duckdb_hugeint *data = NotNullData<duckdb_hugeint>(DUCKDB_TYPE_DECIMAL, chunk, cur_row_idx, col_idx, row_idx);
		return static_cast<int64_t>(data[row_idx].lower);
	}
	int64_t *data = NotNullData<int64_t>(DUCKDB_TYPE_BIGINT, chunk, cur_row_idx, col_idx, row_idx);
	return data[row_idx];
}

template <>
double Result::Value<double>(idx_t col_idx, idx_t row_idx) {
	double *data = NotNullData<double>(DUCKDB_TYPE_DOUBLE, chunk, cur_row_idx, col_idx, row_idx);
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

std::string Result::BinaryValue(idx_t col_idx, idx_t row_idx) {
	duckdb_string_t *data = NotNullData<duckdb_string_t>(DUCKDB_TYPE_BLOB, chunk, cur_row_idx, col_idx, row_idx);
	duckdb_string_t dstr = data[row_idx];
	const char *cstr = duckdb_string_t_data(&dstr);
	uint32_t len = duckdb_string_t_length(dstr);
	return std::string(cstr, len);
}

template <>
duckdb_date_struct Result::Value<duckdb_date_struct>(idx_t col_idx, idx_t row_idx) {
	duckdb_date *data = NotNullData<duckdb_date>(DUCKDB_TYPE_DATE, chunk, cur_row_idx, col_idx, row_idx);
	duckdb_date dt = data[row_idx];
	return duckdb_from_date(dt);
}

template <>
int16_t Result::DecimalValue<int16_t>(idx_t col_idx, idx_t row_idx) {
	if (DBMSConfigured("ClickHouse")) {
		duckdb_hugeint *data = NotNullData<duckdb_hugeint>(DUCKDB_TYPE_DECIMAL, chunk, cur_row_idx, col_idx, row_idx);
		return static_cast<int16_t>(data[row_idx].lower);
	}
	int16_t *data = NotNullData<int16_t>(DUCKDB_TYPE_DECIMAL, chunk, cur_row_idx, col_idx, row_idx);
	return data[row_idx];
}

template <>
int32_t Result::DecimalValue<int32_t>(idx_t col_idx, idx_t row_idx) {
	if (DBMSConfigured("ClickHouse")) {
		duckdb_hugeint *data = NotNullData<duckdb_hugeint>(DUCKDB_TYPE_DECIMAL, chunk, cur_row_idx, col_idx, row_idx);
		return static_cast<int32_t>(data[row_idx].lower);
	}
	int32_t *data = NotNullData<int32_t>(DUCKDB_TYPE_DECIMAL, chunk, cur_row_idx, col_idx, row_idx);
	return data[row_idx];
}

template <>
int64_t Result::DecimalValue<int64_t>(idx_t col_idx, idx_t row_idx) {
	if (DBMSConfigured("ClickHouse")) {
		duckdb_hugeint *data = NotNullData<duckdb_hugeint>(DUCKDB_TYPE_DECIMAL, chunk, cur_row_idx, col_idx, row_idx);
		return static_cast<int64_t>(data[row_idx].lower);
	}
	int64_t *data = NotNullData<int64_t>(DUCKDB_TYPE_DECIMAL, chunk, cur_row_idx, col_idx, row_idx);
	return data[row_idx];
}

template <>
duckdb_hugeint Result::DecimalValue<duckdb_hugeint>(idx_t col_idx, idx_t row_idx) {
	duckdb_hugeint *data = NotNullData<duckdb_hugeint>(DUCKDB_TYPE_DECIMAL, chunk, cur_row_idx, col_idx, row_idx);
	return data[row_idx];
}

bool DBMSConfigured(const std::string dbms_name) {
	char *cstr = std::getenv("ODBC_CONN_STRING");
	if (cstr == nullptr) {
		return dbms_name == "DuckDB";
	}
	std::string str(cstr);
	std::string needle = std::string("{" + dbms_name + " Driver}");
	return str.find(needle) != std::string::npos;
}

bool QuerySuccess(duckdb_result *res, duckdb_state st) {
	if (st != DuckDBSuccess) {
		const char *cerr = duckdb_result_error(res);
		std::string err = cerr != nullptr ? std::string(cerr) : "N/A";
		std::cerr << "Query error, message: [\n" + err + "\n]" << std::endl;
		return false;
	}
	return true;
}

bool PreparedSuccess(duckdb_prepared_statement ps, duckdb_state st) {
	if (st != DuckDBSuccess) {
		const char *cerr = duckdb_prepare_error(ps);
		std::string err = cerr != nullptr ? std::string(cerr) : "N/A";
		std::cerr << "Prepared statement error, message: [\n" + err + "\n]" << std::endl;
		return false;
	}
	return true;
}

std::string CastAsBigintSQL(const std::string &value, const std::string &alias) {
	std::string type_name = "BIGINT";
	std::string postfix = "";
	if (DBMSConfigured("MySQL") || DBMSConfigured("MariaDB")) {
		type_name = "SIGNED";
	} else if (DBMSConfigured("ClickHouse")) {
		type_name = "Nullable(" + type_name + ")";
	} else if (DBMSConfigured("Oracle")) {
		type_name = "NUMBER(18)";
		postfix = " FROM dual";
	} else if (DBMSConfigured("DB2")) {
		postfix = " FROM sysibm.sysdummy1";
	}
	return "CAST(" + value + " AS " + type_name + ") " + alias + postfix;
}

std::string CastAsDateSQL(const std::string &value_in, const std::string &alias) {
	std::string type_name = "DATE";
	std::string postfix = "";
	std::string value = value_in;
	if (DBMSConfigured("ClickHouse")) {
		type_name = "Nullable(" + type_name + ")";
	} else if (DBMSConfigured("Oracle")) {
		if (value != "?") {
			value = "to_date(" + value + ", ''YYYY-MM-DD'')";
		}
		postfix = " FROM dual";
	} else if (DBMSConfigured("DB2")) {
		postfix = " FROM sysibm.sysdummy1";
	}
	return "CAST(" + value + " AS " + type_name + ") " + alias + postfix;
}

std::string CastAsDecimalSQL(const std::string &value, uint8_t precision, uint8_t scale, const std::string &alias) {
	std::string type_name = "DECIMAL(" + std::to_string(precision) + ", " + std::to_string(scale) + ")";
	std::string postfix = "";
	if (DBMSConfigured("ClickHouse")) {
		type_name = "Nullable(" + type_name + ")";
	} else if (DBMSConfigured("Oracle")) {
		postfix = " FROM dual";
	} else if (DBMSConfigured("DB2")) {
		postfix = " FROM sysibm.sysdummy1";
	}
	return "CAST(" + value + " AS " + type_name + ") " + alias + postfix;
}
