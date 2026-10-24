#include "registries.hpp"

#include <mutex>
#include <set>
#include <unordered_map>

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

// initialized from DUCKDB_EXTENSION_ENTRYPOINT
static std::shared_ptr<std::unordered_map<int64_t, SQLHANDLE>> SharedCancellationRegistry() {
	static auto registry = std::make_shared<std::unordered_map<int64_t, SQLHANDLE>>();
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

void CancellationRegistry::Add(int64_t conn_id, HSTMT stmt) {
	auto mtx = SharedMutex();
	std::lock_guard<std::mutex> guard(*mtx);

	auto shared_reg_ptr = SharedCancellationRegistry();
	auto &shared_reg = *shared_reg_ptr;

	shared_reg.emplace(conn_id, stmt);
}

HSTMT CancellationRegistry::Get(int64_t conn_id) {
	auto mtx = SharedMutex();
	std::lock_guard<std::mutex> guard(*mtx);

	auto shared_reg_ptr = SharedCancellationRegistry();
	auto &shared_reg = *shared_reg_ptr;

	auto it = shared_reg.find(conn_id);
	if (it != shared_reg.end()) {
		return it->second;
	}
	return nullptr;
}

void CancellationRegistry::Remove(int64_t conn_id) {
	auto mtx = SharedMutex();
	std::lock_guard<std::mutex> guard(*mtx);

	auto shared_reg_ptr = SharedCancellationRegistry();
	auto &shared_reg = *shared_reg_ptr;

	shared_reg.erase(conn_id);
}

void Registries::Initialize() {
	SharedMutex();
	SharedConnectionsRegistry();
}

} // namespace odbcscanner
