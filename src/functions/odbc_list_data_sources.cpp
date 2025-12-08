#include "odbc_scanner.hpp"

#include <string>
#include <vector>

#include "capi_pointers.hpp"
#include "diagnostics.hpp"
#include "odbc_api.hpp"
#include "scanner_exception.hpp"
#include "strings.hpp"
#include "widechar.hpp"

DUCKDB_EXTENSION_EXTERN

static void odbc_list_data_sources_bind(duckdb_bind_info info) noexcept;
static void odbc_list_data_sources_init(duckdb_init_info info) noexcept;
static void odbc_list_data_sources_local_init(duckdb_init_info info) noexcept;
static void odbc_list_data_sources_function(duckdb_function_info info, duckdb_data_chunk output) noexcept;

namespace odbcscanner {

namespace {

enum class ExecState { UNINITIALIZED, EXECUTED, EXHAUSTED };

enum class DataSourceKind { SYSTEM, USER };

struct DataSourceLength {
	SQLSMALLINT name_len = 0;
	SQLSMALLINT description_len = 0;
};

struct DataSource {
	std::string name;
	std::string description;
	DataSourceKind kind;

	explicit DataSource(std::string name_in, std::string description_in, DataSourceKind kind_in)
	    : name(std::move(name_in)), description(std::move(description_in)), kind(kind_in) {
	}
};

struct LocalInitData {
	ExecState state = ExecState::UNINITIALIZED;
	std::vector<DataSource> data_sources;
	size_t ds_idx = 0;
	duckdb_vector name_vector = nullptr;
	duckdb_vector description_vector = nullptr;
	duckdb_vector kind_vector = nullptr;

	static void Destroy(void *ldata_in) noexcept {
		auto ldata = reinterpret_cast<LocalInitData *>(ldata_in);
		delete ldata;
	}
};

} // namespace

static void Bind(duckdb_bind_info info) {
	auto varchar_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_VARCHAR), LogicalTypeDeleter);

	duckdb_bind_add_result_column(info, "name", varchar_type.get());
	duckdb_bind_add_result_column(info, "description", varchar_type.get());
	duckdb_bind_add_result_column(info, "type", varchar_type.get());
}

static void GlobalInit(duckdb_init_info info) {
	(void)info;
	// no-op
}

static void LocalInit(duckdb_init_info info) {
	auto ldata_ptr = std::unique_ptr<LocalInitData>(new LocalInitData());
	duckdb_init_set_init_data(info, ldata_ptr.release(), LocalInitData::Destroy);
}

static std::vector<DataSourceLength> ReadDataSourceLengths(SQLHANDLE env, SQLSMALLINT first_dir) {
	std::vector<DataSourceLength> res;

	bool first = true;
	for (;;) {
		DataSourceLength dsl;
		dsl.name_len = -1;
		dsl.description_len = -1;
		SQLSMALLINT dir = SQL_FETCH_NEXT;
		if (first) {
			dir = first_dir;
			first = false;
		}

		SQLRETURN ret = SQLDataSourcesW(env, dir, nullptr, 0, &dsl.name_len, nullptr, 0, &dsl.description_len);
		if (ret == SQL_NO_DATA) {
			break;
		}

		if (dsl.name_len == -1 || dsl.description_len == -1) {
			std::string diag = Diagnostics::Read(env, SQL_HANDLE_ENV);
			throw ScannerException("'SQLDataSourcesW' length failed, return: " + std::to_string(ret) +
			                       ", diagnostics: '" + diag + "'");
		}

		res.emplace_back(std::move(dsl));
	}

	return res;
}

static std::vector<DataSource> ReadDataSourcesOfKind(SQLHANDLE env, DataSourceKind kind) {
	SQLSMALLINT first_dir = kind == DataSourceKind::USER ? SQL_FETCH_FIRST_USER : SQL_FETCH_FIRST_SYSTEM;

	int max_attempts = 4;

	for (int attempt = 0; attempt < max_attempts; attempt++) {
		std::vector<DataSourceLength> ds_lengths = ReadDataSourceLengths(env, first_dir);
		std::vector<DataSource> data_sources;
		data_sources.reserve(ds_lengths.size());

		bool first = true;
		bool length_mismatch = false;

		for (DataSourceLength &dsl : ds_lengths) {
			std::vector<SQLWCHAR> name_buf;
			name_buf.resize(dsl.name_len + 1);
			SQLSMALLINT name_len = 0;
			std::vector<SQLWCHAR> desc_buf;
			desc_buf.resize(dsl.description_len + 1);
			SQLSMALLINT desc_len = 0;

			{
				SQLRETURN ret = SQLDataSourcesW(env, first ? first_dir : SQL_FETCH_NEXT, name_buf.data(),
				                                static_cast<SQLSMALLINT>(name_buf.size()), &name_len, desc_buf.data(),
				                                static_cast<SQLSMALLINT>(desc_buf.size()), &desc_len);
				if (first) {
					first = false;
				}
				if (ret == SQL_NO_DATA) {
					length_mismatch = true;
					break;
				}
				if (!SQL_SUCCEEDED(ret)) {
					std::string diag = Diagnostics::Read(env, SQL_HANDLE_ENV);
					throw ScannerException("'SQLDataSourcesW' failed, return: " + std::to_string(ret) +
					                       ", diagnostics: '" + diag + "'");
				}
				if (dsl.name_len != name_len) {
					length_mismatch = true;
					break;
				}
				if (dsl.description_len != desc_len) {
					length_mismatch = true;
					break;
				}
				std::string name = WideChar::Narrow(name_buf.data(), name_buf.size() - 1);
				std::string desc = WideChar::Narrow(desc_buf.data(), desc_buf.size() - 1);
				DataSource ds(std::move(name), std::move(desc), kind);
				data_sources.emplace_back(std::move(ds));
			}
		}

		if (!length_mismatch) {
			return data_sources;
		}
	}

	throw ScannerException("'odbc_list_data_sources' error: unable to read data sources list due to data "
	                       "sources/names/descriptions size mismatch "
	                       "that can be caused by concurrent modifications of the registered data sources, attempts: " +
	                       std::to_string(max_attempts));
}

static std::vector<DataSource> ReadDataSources() {
	SQLHANDLE env_handle = nullptr;
	{
		SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env_handle);
		if (!SQL_SUCCEEDED(ret)) {
			throw ScannerException("'SQLAllocHandle' failed for ENV handle, return: " + std::to_string(ret));
		}
	}

	EnvHandlePtr env(env_handle, EnvHandleDeleter);

	{
		SQLRETURN ret = SQLSetEnvAttr(env.get(), SQL_ATTR_ODBC_VERSION,
		                              reinterpret_cast<SQLPOINTER>(static_cast<uintptr_t>(SQL_OV_ODBC3)), 0);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(env.get(), SQL_HANDLE_ENV);
			throw ScannerException("'SQLSetEnvAttr' failed, return: " + std::to_string(ret) + ", diagnostics: '" +
			                       diag + "'");
		}
	}

	std::vector<DataSource> ds_user = ReadDataSourcesOfKind(env.get(), DataSourceKind::USER);
	std::vector<DataSource> ds_system = ReadDataSourcesOfKind(env.get(), DataSourceKind::SYSTEM);

	std::vector<DataSource> res;
	res.reserve(ds_user.size() + ds_system.size());
	res.insert(res.end(), ds_user.begin(), ds_user.end());
	res.insert(res.end(), ds_system.begin(), ds_system.end());
	return res;
}

static void ListDataSources(duckdb_function_info info, duckdb_data_chunk output) {
	LocalInitData &ldata = *reinterpret_cast<LocalInitData *>(duckdb_function_get_local_init_data(info));

	if (ldata.state == ExecState::EXHAUSTED) {
		duckdb_data_chunk_set_size(output, 0);
		return;
	}

	if (ldata.state == ExecState::UNINITIALIZED) {
		ldata.data_sources = ReadDataSources();
		ldata.state = ExecState::EXECUTED;
		ldata.name_vector = duckdb_data_chunk_get_vector(output, 0);
		ldata.description_vector = duckdb_data_chunk_get_vector(output, 1);
		ldata.kind_vector = duckdb_data_chunk_get_vector(output, 2);

		if (ldata.name_vector == nullptr || ldata.description_vector == nullptr || ldata.kind_vector == nullptr) {
			throw ScannerException("'odbc_list_data_sources' error: invalid null output vector");
		}
	}

	idx_t row_idx = 0;
	for (; row_idx < duckdb_vector_size(); row_idx++) {
		if (ldata.ds_idx >= ldata.data_sources.size()) {
			ldata.state = ExecState::EXHAUSTED;
			break;
		}
		const DataSource &ds = ldata.data_sources.at(ldata.ds_idx++);
		duckdb_vector_assign_string_element_len(ldata.name_vector, row_idx, ds.name.c_str(), ds.name.length());
		duckdb_vector_assign_string_element_len(ldata.description_vector, row_idx, ds.description.c_str(),
		                                        ds.description.length());
		std::string kind = ds.kind == DataSourceKind::USER ? "USER" : "SYSTEM";
		duckdb_vector_assign_string_element_len(ldata.kind_vector, row_idx, kind.c_str(), kind.length());
	}
	duckdb_data_chunk_set_size(output, row_idx);
}

void OdbcListDataSourcesFunction::Register(duckdb_connection conn) {
	auto fun = TableFunctionPtr(duckdb_create_table_function(), TableFunctionDeleter);
	duckdb_table_function_set_name(fun.get(), "odbc_list_data_sources");

	// callbacks
	duckdb_table_function_set_bind(fun.get(), odbc_list_data_sources_bind);
	duckdb_table_function_set_init(fun.get(), odbc_list_data_sources_init);
	duckdb_table_function_set_local_init(fun.get(), odbc_list_data_sources_local_init);
	duckdb_table_function_set_function(fun.get(), odbc_list_data_sources_function);

	// register and cleanup
	duckdb_state state = duckdb_register_table_function(conn, fun.get());

	if (state != DuckDBSuccess) {
		throw ScannerException("'odbc_list_data_sources' function registration failed");
	}
}

} // namespace odbcscanner

static void odbc_list_data_sources_bind(duckdb_bind_info info) noexcept {
	try {
		odbcscanner::Bind(info);
	} catch (std::exception &e) {
		duckdb_bind_set_error(info, e.what());
	}
}

static void odbc_list_data_sources_init(duckdb_init_info info) noexcept {
	try {
		odbcscanner::GlobalInit(info);
	} catch (std::exception &e) {
		duckdb_init_set_error(info, e.what());
	}
}

static void odbc_list_data_sources_local_init(duckdb_init_info info) noexcept {
	try {
		odbcscanner::LocalInit(info);
	} catch (std::exception &e) {
		duckdb_init_set_error(info, e.what());
	}
}

static void odbc_list_data_sources_function(duckdb_function_info info, duckdb_data_chunk output) noexcept {
	try {
		odbcscanner::ListDataSources(info, output);
	} catch (std::exception &e) {
		duckdb_function_set_error(info, e.what());
	}
}
