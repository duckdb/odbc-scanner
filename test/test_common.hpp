#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// Must be included before catch.hpp, that includes windows.h, that defines macros
// (like DELETE and IGNORE) conflicting with the names used in C++ API
#include "duckdb_cpp.hpp"

#include "catch.hpp"

using idx_t = duckdb::cxx::idx_t;
using int128_t = duckdb::cxx::int128_t;

using ValuePtr = std::unique_ptr<duckdb::cxx::Value>;

// Replaces 'duckdb_date_struct' from C API V1
struct DateStruct {
	int32_t year;
	int8_t month;
	int8_t day;
};

struct ScannerConn {
	duckdb::cxx::Instance db;
	duckdb::cxx::Connection conn;
	bool odbc_connection_established = false;

	ScannerConn(bool establish_odbc_connection = true);

	~ScannerConn() noexcept;
};

// Query result, all chunks are fetched when the query is executed
struct Result {
	std::string error;
	std::vector<duckdb::cxx::LogicalTypeId> column_types;
	std::vector<duckdb::cxx::DataChunk> chunks;
	duckdb::cxx::DataChunk *chunk = nullptr;
	idx_t next_chunk_idx = 0;
	idx_t cur_row_idx = 0;

	void Fill(duckdb::cxx::QueryResult res);

	const std::string &Error();

	bool NextChunk();

	bool IsNull(idx_t col_idx, idx_t row_idx);

	template<typename T>
	T Value(idx_t col_idx, idx_t row_idx);

	std::string BinaryValue(idx_t col_idx, idx_t row_idx);

	template<typename T>
	T DecimalValue(idx_t col_idx, idx_t row_idx);
};

// Parameters are bound to prepared statement on execution in C API V2,
// bound values are borrowed and must be alive when the statement is executed
struct PreparedStatement {
	std::unique_ptr<duckdb::cxx::PreparedStatement> ps;
	duckdb::cxx::Factory *factory = nullptr;
	std::string error;
	std::vector<const duckdb::cxx::Value *> params;

	bool Bind(idx_t param_idx, const duckdb::cxx::Value *value);

	bool Execute(Result &res);
};

bool Query(duckdb::cxx::Connection &conn, const std::string &sql, Result &res);

bool Prepare(duckdb::cxx::Connection &conn, const std::string &sql, PreparedStatement &ps);

ValuePtr CreateInt32(ScannerConn &sc, int32_t value);

ValuePtr CreateVarchar(ScannerConn &sc, const std::string &value);

ValuePtr CreateDate(ScannerConn &sc, DateStruct value);

ValuePtr CreateDecimal(ScannerConn &sc, int128_t value, uint8_t width, uint8_t scale);

bool DBMSConfigured(const std::string dbms_name);

bool QuerySuccess(Result &res, bool st);

bool PreparedSuccess(PreparedStatement &ps, bool st);

std::string CastAsBigintSQL(const std::string& value, const std::string &alias = "");

std::string CastAsDateSQL(const std::string& value, const std::string &alias = "");

std::string CastAsDecimalSQL(const std::string& value, uint8_t precision, uint8_t scale, const std::string &alias = "");

std::string IfExistsSQL();

std::string ProjectRootDir();
