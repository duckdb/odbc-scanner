#define CATCH_CONFIG_MAIN
#include "test_common.hpp"

#include <cstring>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif // __APPLE__

#define SCANNER_QUOTE(value)                 #value
#define SCANNER_STR(value)                   SCANNER_QUOTE(value)
#define ODBC_SCANNER_EXTENSION_FILE_PATH_STR SCANNER_STR(ODBC_SCANNER_EXTENSION_FILE_PATH)

static duckdb::cxx::StartupOptions ScannerStartupOptions() {
	duckdb::cxx::StartupOptions options;
	options.EnableUnsignedExtensions(true);
	return options;
}

ScannerConn::ScannerConn(bool establish_odbc_connection) : db(":memory:", ScannerStartupOptions()), conn(db.Connect()) {
	db.GetConfig().SetOption("threads", "1");

	Result res_load;
	std::string load_query = "LOAD '" + std::string(ODBC_SCANNER_EXTENSION_FILE_PATH_STR) + "'";
	bool state_load = Query(conn, load_query, res_load);
	REQUIRE(QuerySuccess(res_load, state_load));

	if (!establish_odbc_connection) {
		return;
	}

	char *conn_cstr = std::getenv("ODBC_CONN_STRING");
	std::string conn_str = conn_cstr != nullptr ? std::string(conn_cstr) : "Driver={DuckDB Driver};threads=1;";

	std::string sql = std::string() + "SET VARIABLE conn = odbc_connect('" + conn_str + "')";
	Result conn_res;
	bool state_odbc_conn = Query(conn, sql, conn_res);
	if (!state_odbc_conn) {
		std::cerr << conn_res.Error() << std::endl;
	}
	REQUIRE(state_odbc_conn);
	this->odbc_connection_established = true;
}

ScannerConn::~ScannerConn() noexcept {
	if (odbc_connection_established) {
		Result res_close;
		bool state_close = Query(conn, "SELECT odbc_close(getvariable('conn'))", res_close);
		REQUIRE(state_close);
	}
}

void Result::Fill(duckdb::cxx::QueryResult res) {
	duckdb::cxx::Schema schema = res.GetSchema();
	for (idx_t i = 0; i < schema.GetFieldCount(); i++) {
		column_types.push_back(schema.GetFieldType(i).GetTypeId());
	}
	// fetch all chunks to report execution errors right away
	for (;;) {
		duckdb::cxx::DataChunk fetched = res.FetchChunk();
		if (!fetched) {
			break;
		}
		chunks.emplace_back(std::move(fetched));
	}
}

const std::string &Result::Error() {
	return error;
}

bool Result::NextChunk() {
	if (chunk != nullptr) {
		cur_row_idx += chunk->GetRowCount();
	}
	if (next_chunk_idx >= chunks.size()) {
		this->chunk = nullptr;
		return false;
	}
	this->chunk = &chunks.at(next_chunk_idx);
	next_chunk_idx += 1;
	return true;
}

static duckdb::cxx::VectorView ChunkVectorView(duckdb::cxx::DataChunk *chunk, idx_t cur_row_idx, idx_t col_idx,
                                               idx_t row_idx, idx_t &phys_row_idx) {
	REQUIRE(chunk != nullptr);

	idx_t rel_row_idx = row_idx - cur_row_idx;
	REQUIRE(rel_row_idx < chunk->GetRowCount());

	REQUIRE(col_idx < chunk->GetVectorCount());

	duckdb::cxx::Vector vec = chunk->GetVector(col_idx);
	REQUIRE(vec);
	if (vec.GetVectorType() == duckdb::cxx::VectorType::OTHER) {
		vec.Flatten();
	}
	duckdb::cxx::VectorView view = vec.GetView();

	phys_row_idx = view.SelAt(rel_row_idx);
	return view;
}

bool Result::IsNull(idx_t col_idx, idx_t row_idx) {
	idx_t phys_row_idx = 0;
	duckdb::cxx::VectorView view = ChunkVectorView(chunk, cur_row_idx, col_idx, row_idx, phys_row_idx);
	return !view.RowIsValid(phys_row_idx);
}

template <typename CTYPE>
static const CTYPE &NotNullData(duckdb::cxx::LogicalTypeId expected_type_id, Result &res, idx_t col_idx,
                                idx_t row_idx) {
	REQUIRE(col_idx < res.column_types.size());
	REQUIRE(res.column_types.at(col_idx) == expected_type_id);

	idx_t phys_row_idx = 0;
	duckdb::cxx::VectorView view = ChunkVectorView(res.chunk, res.cur_row_idx, col_idx, row_idx, phys_row_idx);
	REQUIRE(view.RowIsValid(phys_row_idx));

	return view.Data<CTYPE>()[phys_row_idx];
}

static duckdb::cxx::LogicalTypeId ColumnType(Result &res, idx_t col_idx) {
	REQUIRE(col_idx < res.column_types.size());
	return res.column_types.at(col_idx);
}

// Proleptic Gregorian calendar conversions, see:
// https://howardhinnant.github.io/date_algorithms.html

static DateStruct FromDate(int32_t days) {
	int64_t z = static_cast<int64_t>(days) + 719468;
	int64_t era = (z >= 0 ? z : z - 146096) / 146097;
	int64_t doe = z - era * 146097;
	int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	int64_t y = yoe + era * 400;
	int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	int64_t mp = (5 * doy + 2) / 153;
	int64_t d = doy - (153 * mp + 2) / 5 + 1;
	int64_t m = mp < 10 ? mp + 3 : mp - 9;

	DateStruct res;
	res.year = static_cast<int32_t>(y + (m <= 2 ? 1 : 0));
	res.month = static_cast<int8_t>(m);
	res.day = static_cast<int8_t>(d);
	return res;
}

static int32_t ToDate(DateStruct date) {
	int64_t m = static_cast<int64_t>(date.month);
	int64_t d = static_cast<int64_t>(date.day);
	int64_t y = static_cast<int64_t>(date.year) - (m <= 2 ? 1 : 0);
	int64_t era = (y >= 0 ? y : y - 399) / 400;
	int64_t yoe = y - era * 400;
	int64_t doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
	int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return static_cast<int32_t>(era * 146097 + doe - 719468);
}

using duckdb::cxx::LogicalTypeId;

template <>
uint8_t Result::Value<uint8_t>(idx_t col_idx, idx_t row_idx) {
	return NotNullData<uint8_t>(LogicalTypeId::UTINYINT, *this, col_idx, row_idx);
}

template <>
int16_t Result::Value<int16_t>(idx_t col_idx, idx_t row_idx) {
	return NotNullData<int16_t>(LogicalTypeId::SMALLINT, *this, col_idx, row_idx);
}

template <>
int32_t Result::Value<int32_t>(idx_t col_idx, idx_t row_idx) {
	return NotNullData<int32_t>(LogicalTypeId::INTEGER, *this, col_idx, row_idx);
}

template <>
int64_t Result::Value<int64_t>(idx_t col_idx, idx_t row_idx) {
	if (DBMSConfigured("MariaDB") && LogicalTypeId::INTEGER == ColumnType(*this, col_idx)) {
		return NotNullData<int32_t>(LogicalTypeId::INTEGER, *this, col_idx, row_idx);
	}
	if (DBMSConfigured("Oracle")) {
		return NotNullData<int64_t>(LogicalTypeId::DECIMAL, *this, col_idx, row_idx);
	}
	if (DBMSConfigured("Snowflake")) {
		const int128_t &data = NotNullData<int128_t>(LogicalTypeId::DECIMAL, *this, col_idx, row_idx);
		return static_cast<int64_t>(data.lower);
	}
	return NotNullData<int64_t>(LogicalTypeId::BIGINT, *this, col_idx, row_idx);
}

template <>
double Result::Value<double>(idx_t col_idx, idx_t row_idx) {
	return NotNullData<double>(LogicalTypeId::DOUBLE, *this, col_idx, row_idx);
}

template <>
bool Result::Value<bool>(idx_t col_idx, idx_t row_idx) {
	return NotNullData<bool>(LogicalTypeId::BOOLEAN, *this, col_idx, row_idx);
}

template <>
std::string Result::Value<std::string>(idx_t col_idx, idx_t row_idx) {
	return std::string(NotNullData<duckdb::cxx::varchar_t>(LogicalTypeId::VARCHAR, *this, col_idx, row_idx).view());
}

std::string Result::BinaryValue(idx_t col_idx, idx_t row_idx) {
	return std::string(NotNullData<duckdb::cxx::blob_t>(LogicalTypeId::BLOB, *this, col_idx, row_idx).view());
}

template <>
DateStruct Result::Value<DateStruct>(idx_t col_idx, idx_t row_idx) {
	duckdb::cxx::date_t dt = NotNullData<duckdb::cxx::date_t>(LogicalTypeId::DATE, *this, col_idx, row_idx);
	return FromDate(dt.days);
}

template <>
int16_t Result::DecimalValue<int16_t>(idx_t col_idx, idx_t row_idx) {
	if (DBMSConfigured("ClickHouse")) {
		const int128_t &data = NotNullData<int128_t>(LogicalTypeId::DECIMAL, *this, col_idx, row_idx);
		return static_cast<int16_t>(data.lower);
	}
	return NotNullData<int16_t>(LogicalTypeId::DECIMAL, *this, col_idx, row_idx);
}

template <>
int32_t Result::DecimalValue<int32_t>(idx_t col_idx, idx_t row_idx) {
	if (DBMSConfigured("ClickHouse")) {
		const int128_t &data = NotNullData<int128_t>(LogicalTypeId::DECIMAL, *this, col_idx, row_idx);
		return static_cast<int32_t>(data.lower);
	}
	return NotNullData<int32_t>(LogicalTypeId::DECIMAL, *this, col_idx, row_idx);
}

template <>
int64_t Result::DecimalValue<int64_t>(idx_t col_idx, idx_t row_idx) {
	if (DBMSConfigured("ClickHouse")) {
		const int128_t &data = NotNullData<int128_t>(LogicalTypeId::DECIMAL, *this, col_idx, row_idx);
		return static_cast<int64_t>(data.lower);
	}
	return NotNullData<int64_t>(LogicalTypeId::DECIMAL, *this, col_idx, row_idx);
}

template <>
int128_t Result::DecimalValue<int128_t>(idx_t col_idx, idx_t row_idx) {
	return NotNullData<int128_t>(LogicalTypeId::DECIMAL, *this, col_idx, row_idx);
}

bool PreparedStatement::Bind(idx_t param_idx, const duckdb::cxx::Value *value) {
	if (param_idx == 0 || value == nullptr) {
		this->error = "Invalid parameter, index: " + std::to_string(param_idx);
		return false;
	}
	if (params.size() < param_idx) {
		params.resize(param_idx, nullptr);
	}
	params[param_idx - 1] = value;
	return true;
}

bool PreparedStatement::Execute(Result &res) {
	try {
		// values are passed to the statement by value, bound values are copied
		std::vector<duckdb::cxx::Value> values;
		for (const duckdb::cxx::Value *val : params) {
			REQUIRE(val != nullptr);
			values.emplace_back(val->Cast(*factory, val->GetLogicalType()));
		}
		res.Fill(ps->Execute(values));
		return true;
	} catch (std::exception &e) {
		res.error = e.what();
		return false;
	}
}

// Parses the specified SQL, returns the last statement, all previous statements are executed
static duckdb::cxx::SqlStatement ParseAndExecuteAllButLast(duckdb::cxx::Connection &conn, const std::string &sql) {
	auto iter = conn.ParseSQL(sql);
	duckdb::cxx::SqlStatement last = iter.Next();
	if (!last) {
		throw std::runtime_error("No statements to execute");
	}
	for (;;) {
		duckdb::cxx::SqlStatement stmt = iter.Next();
		if (!stmt) {
			break;
		}
		conn.Execute(last).Drain();
		last = std::move(stmt);
	}
	return last;
}

bool Query(duckdb::cxx::Connection &conn, const std::string &sql, Result &res) {
	try {
		duckdb::cxx::SqlStatement stmt = ParseAndExecuteAllButLast(conn, sql);
		res.Fill(conn.Execute(stmt));
		return true;
	} catch (std::exception &e) {
		res.error = e.what();
		return false;
	}
}

bool Prepare(duckdb::cxx::Connection &conn, const std::string &sql, PreparedStatement &ps) {
	try {
		duckdb::cxx::SqlStatement stmt = ParseAndExecuteAllButLast(conn, sql);
		ps.ps.reset(new duckdb::cxx::PreparedStatement(conn.Prepare(stmt)));
		ps.factory = &conn.GetFactory();
		return true;
	} catch (std::exception &e) {
		ps.error = e.what();
		return false;
	}
}

ValuePtr CreateInt32(ScannerConn &sc, int32_t value) {
	return ValuePtr(new duckdb::cxx::Value(duckdb::cxx::Value::Create(sc.conn.GetFactory(), value)));
}

ValuePtr CreateVarchar(ScannerConn &sc, const std::string &value) {
	duckdb::cxx::varchar_t str(value);
	return ValuePtr(new duckdb::cxx::Value(duckdb::cxx::Value::Create(sc.conn.GetFactory(), str)));
}

ValuePtr CreateDate(ScannerConn &sc, DateStruct value) {
	duckdb::cxx::date_t dt;
	dt.days = ToDate(value);
	return ValuePtr(new duckdb::cxx::Value(duckdb::cxx::Value::Create(sc.conn.GetFactory(), dt)));
}

ValuePtr CreateDecimal(ScannerConn &sc, int128_t value, uint8_t width, uint8_t scale) {
	duckdb::cxx::Factory &factory = sc.conn.GetFactory();
	// DECIMAL values are created with the width and scale known at compile time
	if (width == 4 && scale == 3) {
		duckdb::cxx::decimal_t<4, 3> dec {static_cast<int16_t>(value.lower)};
		return ValuePtr(new duckdb::cxx::Value(duckdb::cxx::Value::Create(factory, dec)));
	} else if (width == 38 && scale == 3) {
		duckdb::cxx::decimal_t<38, 3> dec {value};
		return ValuePtr(new duckdb::cxx::Value(duckdb::cxx::Value::Create(factory, dec)));
	}
	FAIL("Unsupported DECIMAL width: " + std::to_string(width) + ", scale: " + std::to_string(scale));
	return ValuePtr();
}

bool DBMSConfigured(const std::string dbms_name) {
	char *cstr = std::getenv("ODBC_CONN_STRING");
	if (cstr == nullptr) {
		return dbms_name == "DuckDB";
	}
	std::string str(cstr);
	if (dbms_name == "Firebird") {
		std::string needle_alt = "{Firebird ODBC Driver}";
		std::string needle = "{Firebird Driver}";
		return str.find(needle_alt) != std::string::npos || str.find(needle) != std::string::npos;
	}
	std::string needle = std::string("{" + dbms_name + " Driver}");
	return str.find(needle) != std::string::npos;
}

bool QuerySuccess(Result &res, bool st) {
	if (!st) {
		std::cerr << "Query error, message: [\n" + res.Error() + "\n]" << std::endl;
		return false;
	}
	return true;
}

bool PreparedSuccess(PreparedStatement &ps, bool st) {
	if (!st) {
		std::cerr << "Prepared statement error, message: [\n" + ps.error + "\n]" << std::endl;
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
	} else if (DBMSConfigured("Firebird")) {
		postfix = " FROM rdb$database";
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
	} else if (DBMSConfigured("Firebird")) {
		postfix = " FROM rdb$database";
	} else if (DBMSConfigured("Informix")) {
		if (value != "?") {
			value = "(" + value + "::DATETIME YEAR TO DAY)::DATE";
		}
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
	} else if (DBMSConfigured("Firebird")) {
		postfix = " FROM rdb$database";
	}
	return "CAST(" + value + " AS " + type_name + ") " + alias + postfix;
}

std::string IfExistsSQL() {
	if (DBMSConfigured("Oracle")) {
		return "";
	} else {
		return "IF EXISTS";
	}
}

#if defined(__linux__)
static std::string CurrentExecutablePathLinux() {
	std::string res;
	ssize_t size = 64;
	for (;;) {
		res.resize(size);
		ssize_t res_size = readlink("/proc/self/exe", std::addressof(res.front()), size);
		REQUIRE(res_size >= 0);
		if (res_size < size) {
			res.resize(res_size);
			break;
		}
		size = size * 2;
	}
	return res;
}
#endif // Linux

#if defined(_WIN32)
static std::string CurrentExecutablePathWindows() {
	DWORD size = 64;
	std::string out;
	for (;;) {
		out.resize(size);
		auto path = std::addressof(out.front());
		auto res_size = GetModuleFileNameA(NULL, path, size);
		REQUIRE(res_size >= 0);
		if (res_size < size) {
			out.resize(res_size);
			return out;
		}
		size = size * 2;
	}
}
#endif // !_WIN32

#if defined(__APPLE__)
static std::string CurrentExecutablePathMac() {
	std::string out;
	uint32_t size = 64;
	out.resize(size);
	char *path = std::addressof(out.front());
	int res = _NSGetExecutablePath(path, &size);
	REQUIRE(-1 != res);
	if (0 == res) {
		// trim null terminated buffer
		return std::string(out.c_str());
	} else {
		out.resize(size);
		path = std::addressof(out.front());
		res = _NSGetExecutablePath(path, &size);
		REQUIRE(0 == res);
		// trim null terminated buffer
		return std::string(out.c_str());
	}
}
#endif // __APPLE__

std::string ProjectRootDir() {
	std::string exec_path =
#if defined(__linux__)
	    CurrentExecutablePathLinux();
#elif defined(_WIN32)
	    CurrentExecutablePathWindows();
#elif defined(__APPLE__)
	    CurrentExecutablePathMac();
#else
	    REQUIRE(false);
#endif
	std::replace(exec_path.begin(), exec_path.end(), '\\', '/');
	size_t slash_pos = exec_path.find_last_of('/');
	std::string exec_dir = exec_path.substr(0, slash_pos);
	return exec_dir + "/../../..";
}
