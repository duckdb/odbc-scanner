#include "registries.hpp"

#include <mutex>
#include <set>

#include "scanner_exception.hpp"

namespace odbcscanner {

// initialized from DUCKDB_EXTENSION_ENTRYPOINT
static std::shared_ptr<std::mutex> SharedMutex() {
	static auto mutex = std::make_shared<std::mutex>();
	return mutex;
}

// We are NOT closing the connection on process exit - some
// ODBC drivers don't like it and can complain to stderr or crash
static void SharedConectionsRegistryDeleter(std::set<int64_t> *reg_ptr) {
	auto &reg = *reg_ptr;
	/*
	for (int64_t conn_id : reg) {
	    OdbcConnection *conn_ptr = reinterpret_cast<OdbcConnection *>(conn_id);
	    delete conn_ptr;
	}
	*/
	reg.clear();
	delete reg_ptr;
}

// initialized from DUCKDB_EXTENSION_ENTRYPOINT
static std::shared_ptr<std::set<int64_t>> SharedConnectionsRegistry() {
	static auto registry = std::shared_ptr<std::set<int64_t>>(new std::set<int64_t>, SharedConectionsRegistryDeleter);
	return registry;
}

static void SharedParamsRegistryDeleter(std::set<int64_t> *reg_ptr) {
	auto &reg = *reg_ptr;
	for (int64_t params_id : reg) {
		std::vector<ScannerParam> *params_ptr = reinterpret_cast<std::vector<ScannerParam> *>(params_id);
		delete params_ptr;
	}
	reg.clear();
	delete reg_ptr;
}

// initialized from DUCKDB_EXTENSION_ENTRYPOINT
static std::shared_ptr<std::set<int64_t>> SharedParamsRegistry() {
	static auto registry = std::shared_ptr<std::set<int64_t>>(new std::set<int64_t>, SharedParamsRegistryDeleter);
	return registry;
}

int64_t ConnectionsRegistry::Add(std::unique_ptr<OdbcConnection> conn) {
	if (conn.get() == nullptr) {
		throw ScannerException("Cannot register invalid empty connection");
	}

	auto mtx = SharedMutex();
	std::lock_guard<std::mutex> guard(*mtx);

	auto shared_reg_ptr = SharedConnectionsRegistry();
	auto &shared_reg = *shared_reg_ptr;

	int64_t conn_id = reinterpret_cast<int64_t>(conn.get());
	auto res = shared_reg.insert(conn_id);
	bool inserted = res.second;

	if (!inserted) {
		throw ScannerException("ODBC connection is already registered, ID: " + std::to_string(conn_id));
	}

	conn.release();

	return conn_id;
}

std::unique_ptr<OdbcConnection> ConnectionsRegistry::Remove(int64_t conn_id) {
	auto mtx = SharedMutex();
	std::lock_guard<std::mutex> guard(*mtx);

	auto shared_reg_ptr = SharedConnectionsRegistry();
	auto &shared_reg = *shared_reg_ptr;

	auto removed_count = shared_reg.erase(conn_id);
	if (removed_count == 0) {
		return std::unique_ptr<OdbcConnection>(nullptr);
	}

	OdbcConnection *conn_ptr = reinterpret_cast<OdbcConnection *>(conn_id);
	return std::unique_ptr<OdbcConnection>(conn_ptr);
}

int64_t ParamsRegistry::Add(std::unique_ptr<std::vector<ScannerParam>> params) {
	if (params.get() == nullptr) {
		throw ScannerException("Cannot register invalid empty params");
	}

	auto mtx = SharedMutex();
	std::lock_guard<std::mutex> guard(*mtx);

	auto shared_reg_ptr = SharedParamsRegistry();
	auto &shared_reg = *shared_reg_ptr;

	int64_t params_id = reinterpret_cast<int64_t>(params.get());
	auto res = shared_reg.insert(params_id);
	bool inserted = res.second;

	if (!inserted) {
		throw ScannerException("Parameters are already registered, ID: " + std::to_string(params_id));
	}

	params.release();

	return params_id;
}

std::unique_ptr<std::vector<ScannerParam>> ParamsRegistry::Remove(int64_t params_id) {
	auto mtx = SharedMutex();
	std::lock_guard<std::mutex> guard(*mtx);

	auto shared_reg_ptr = SharedParamsRegistry();
	auto &shared_reg = *shared_reg_ptr;

	auto removed_count = shared_reg.erase(params_id);
	if (removed_count == 0) {
		return std::unique_ptr<std::vector<ScannerParam>>(nullptr);
	}

	std::vector<ScannerParam> *params_ptr = reinterpret_cast<std::vector<ScannerParam> *>(params_id);
	return std::unique_ptr<std::vector<ScannerParam>>(params_ptr);
}

void Registries::Initialize() {
	SharedMutex();
	SharedConnectionsRegistry();
	SharedParamsRegistry();
}

} // namespace odbcscanner
