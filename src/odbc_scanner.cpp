#include "odbc_scanner.hpp"

#include <stdexcept>
#include <string>

#include "capi_pointers.hpp"
#include "registries.hpp"
#include "scanner_exception.hpp"

namespace odbcscanner {

int64_t OdbcScanner::timezone_offset_seconds = 0;

static int64_t FetchTimezoneOffsetSeconds(duckdb_connection connection) {
	duckdb_result res_val;
	std::string sql = "SELECT timezone(current_timestamp)";

	duckdb_state st = duckdb_query(connection, sql.c_str(), &res_val);
	auto res = ResultPtr(&res_val, ResultDeleter);
	if (st != DuckDBSuccess) {
		const char *err = duckdb_result_error(res.get());
		throw ScannerException("timezone init failure, query: '" + sql + "', message: '" + err + "'");
	}

	auto chunk = DataChunkPtr(duckdb_fetch_chunk(*res), DataChunkDeleter);
	if (chunk.get() == nullptr) {
		return 0;
	}

	duckdb_vector vec = duckdb_data_chunk_get_vector(chunk.get(), 0);
	if (vec == nullptr) {
		return 0;
	}

	uint64_t *validity = duckdb_vector_get_validity(vec);
	if (validity != nullptr && !duckdb_validity_row_is_valid(validity, 0)) {
		return 0;
	}

	int64_t *data = reinterpret_cast<int64_t *>(duckdb_vector_get_data(vec));
	if (data == nullptr) {
		return 0;
	}

	return data[0];
}

static void Initialize(duckdb_connection connection, duckdb_extension_info, duckdb_extension_access *) {
	OdbcScanner::timezone_offset_seconds = FetchTimezoneOffsetSeconds(connection);
	Registries::Initialize();
	OdbcBindParamsFunction::Register(connection);
	OdbcCloseFunction::Register(connection);
	OdbcConnectFunction::Register(connection);
	OdbcCreateParamsFunction::Register(connection);
	OdbcQueryFunction::Register(connection);
}

} // namespace odbcscanner

bool initiaize_odbc_scanner(duckdb_connection connection, duckdb_extension_info info,
                            duckdb_extension_access *access) /* noexcept */ {
	try {
		odbcscanner::Initialize(connection, info, access);
		return true;
	} catch (std::exception &e) {
		std::string msg = "ODBC Scanner initialization failed: " + std::string(e.what());
		access->set_error(info, msg.c_str());
		return false;
	}
}
