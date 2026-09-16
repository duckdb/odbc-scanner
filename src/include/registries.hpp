#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "connection.hpp"
#include "odbc_api.hpp"
#include "params.hpp"

namespace odbcscanner {

struct ConnectionsRegistry {
	static int64_t Add(std::unique_ptr<OdbcConnection> conn);

	static std::unique_ptr<OdbcConnection> Remove(int64_t conn_id);
};

struct CancellationRegistry {
	static void Add(int64_t conn_id, HSTMT stmt);

	static HSTMT Get(int64_t conn_id);

	static void Remove(int64_t conn_id);
};

struct ParamsRegistry {
	static int64_t Add(std::unique_ptr<std::vector<ScannerValue>> params);

	static std::unique_ptr<std::vector<ScannerValue>> Remove(int64_t params_id);
};

struct Registries {
	static void Initialize();
};

} // namespace odbcscanner
