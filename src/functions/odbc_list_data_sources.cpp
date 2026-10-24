#include "odbc_scanner.hpp"

#include <string>
#include <vector>

#include "function_args.hpp"
#include "diagnostics.hpp"
#include "driver_manager.hpp"
#include "make_unique.hpp"
#include "odbc_api.hpp"
#include "scanner_exception.hpp"
#include "strings.hpp"
#include "widechar.hpp"

static void odbc_list_data_sources_bind(duckdb::cxx::TableFunction::BindInput &input);
static void odbc_list_data_sources_init(duckdb::cxx::TableFunction::InitGlobalInput &input);
static void odbc_list_data_sources_local_init(duckdb::cxx::TableFunction::InitLocalInput &input);
static void odbc_list_data_sources_function(duckdb::cxx::TableFunction::ExecInput &input);

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
};

} // namespace

static void Bind(TableFunction::BindInput &input) {
	Factory &factory = input.GetContext().GetFactory();
	auto varchar_type = factory.CreateType(LogicalTypeId::VARCHAR);

	input.AddResultColumn("name", varchar_type);
	input.AddResultColumn("description", varchar_type);
	input.AddResultColumn("type", varchar_type);
}

static void GlobalInit(TableFunction::InitGlobalInput &input) {
	(void)input;
	// no-op
}

static void LocalInit(TableFunction::InitLocalInput &input) {
	input.SetLocalState<LocalInitData>();
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
	DriverManager::Load();

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

static void ListDataSources(TableFunction::ExecInput &input, DataChunk &output) {
	LocalInitData &ldata = input.GetLocalState<LocalInitData>();

	if (ldata.state == ExecState::EXHAUSTED) {
		output.GetVector(0).SetSize(0);
		return;
	}

	if (ldata.state == ExecState::UNINITIALIZED) {
		ldata.data_sources = ReadDataSources();
		ldata.state = ExecState::EXECUTED;
	}

	Vector name_vector = output.GetVector(0);
	Vector description_vector = output.GetVector(1);
	Vector kind_vector = output.GetVector(2);

	if (!name_vector || !description_vector || !kind_vector) {
		throw ScannerException("'odbc_list_data_sources' error: invalid null output vector");
	}

	idx_t vector_size = output.GetCapacity();
	idx_t row_idx = 0;
	for (; row_idx < vector_size; row_idx++) {
		if (ldata.ds_idx >= ldata.data_sources.size()) {
			ldata.state = ExecState::EXHAUSTED;
			break;
		}
		const DataSource &ds = ldata.data_sources.at(ldata.ds_idx++);
		name_vector.AssignStringUnsafe(row_idx, std::string_view(ds.name.c_str(), ds.name.length()));
		description_vector.AssignStringUnsafe(row_idx,
		                                      std::string_view(ds.description.c_str(), ds.description.length()));
		std::string kind = ds.kind == DataSourceKind::USER ? "USER" : "SYSTEM";
		kind_vector.AssignStringUnsafe(row_idx, std::string_view(kind.c_str(), kind.length()));
	}
	output.GetVector(0).SetSize(row_idx);
}

void OdbcListDataSourcesFunction::Register(Extension &extension, Factory &factory) {
	auto fun = TableFunction::Create(factory);
	fun.SetName("odbc_list_data_sources");

	// callbacks
	fun.SetBindCallback(odbc_list_data_sources_bind);
	fun.SetInitGlobalCallback(odbc_list_data_sources_init);
	fun.SetInitLocalCallback(odbc_list_data_sources_local_init);
	fun.SetExecCallback(odbc_list_data_sources_function);

	// register and cleanup
	try {
		extension.Register(fun);
	} catch (std::exception &e) {
		throw ScannerException(std::string("\'") + "odbc_list_data_sources" +
		                       "\' function registration failed: " + e.what());
	}
}

} // namespace odbcscanner

static void odbc_list_data_sources_bind(duckdb::cxx::TableFunction::BindInput &input) {
	try {
		odbcscanner::Bind(input);
	} catch (std::exception &e) {
		throw odbcscanner::FunctionArgs::BinderError(e.what());
	}
}

static void odbc_list_data_sources_init(duckdb::cxx::TableFunction::InitGlobalInput &input) {
	odbcscanner::GlobalInit(input);
}

static void odbc_list_data_sources_local_init(duckdb::cxx::TableFunction::InitLocalInput &input) {
	odbcscanner::LocalInit(input);
}

static void odbc_list_data_sources_function(duckdb::cxx::TableFunction::ExecInput &input) {
	duckdb::cxx::DataChunk output = input.GetOutputChunk();
	odbcscanner::ListDataSources(input, output);
}
