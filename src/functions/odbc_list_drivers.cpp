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

static void odbc_list_drivers_bind(duckdb::cxx::TableFunction::BindInput &input);
static void odbc_list_drivers_init(duckdb::cxx::TableFunction::InitGlobalInput &input);
static void odbc_list_drivers_local_init(duckdb::cxx::TableFunction::InitLocalInput &input);
static void odbc_list_drivers_function(duckdb::cxx::TableFunction::ExecInput &input);

namespace odbcscanner {

namespace {

enum class ExecState { UNINITIALIZED, EXECUTED, EXHAUSTED };

struct DriverAttribute {
	std::string name;
	std::string value;

	explicit DriverAttribute(std::string name_in, std::string value_in)
	    : name(std::move(name_in)), value(std::move(value_in)) {
	}
};

struct DriverLength {
	SQLSMALLINT description_len = 0;
	SQLSMALLINT attributes_len = 0;
};

struct Driver {
	std::string description;
	std::vector<DriverAttribute> attributes;

	explicit Driver(std::string description_in, std::vector<DriverAttribute> attributes_in)
	    : description(std::move(description_in)), attributes(std::move(attributes_in)) {
	}
};

struct LocalInitData {
	ExecState state = ExecState::UNINITIALIZED;
	std::vector<Driver> drivers;
	size_t driver_idx = 0;
};

} // namespace

static void Bind(TableFunction::BindInput &input) {
	Factory &factory = input.GetContext().GetFactory();
	auto varchar_type = factory.CreateType(LogicalTypeId::VARCHAR);
	auto map_type = factory.ParseType("MAP(VARCHAR, VARCHAR)");

	input.AddResultColumn("description", varchar_type);
	input.AddResultColumn("attributes", map_type);
}

static void GlobalInit(TableFunction::InitGlobalInput &input) {
	(void)input;
	// no-op
}

static void LocalInit(TableFunction::InitLocalInput &input) {
	input.SetLocalState<LocalInitData>();
}

static std::vector<DriverAttribute> ParseAttributes(const std::string attrs_str) {
	std::vector<DriverAttribute> res;
	if (attrs_str.length() < 3) {
		return res;
	}
	std::string as = std::string(attrs_str.data(), attrs_str.length() - 2);
	std::vector<std::string> entries = Strings::Split(attrs_str, '\0');
	for (const std::string &en : entries) {
		std::vector<std::string> parts = Strings::Split(en, '=');
		std::string name = parts.size() > 0 ? parts[0] : "INVALID_STRING";
		std::string value = parts.size() > 1 ? parts[1] : "INVALID_STRING";
		DriverAttribute att(std::move(name), std::move(value));
		res.emplace_back(std::move(att));
	}
	return res;
}

static std::vector<DriverLength> ReadDriverLengths(SQLHANDLE env) {
	std::vector<DriverLength> res;

	for (;;) {
		DriverLength dl;
		dl.description_len = -1;
		dl.attributes_len = -1;

		SQLRETURN ret =
		    SQLDriversW(env, SQL_FETCH_NEXT, nullptr, 0, &dl.description_len, nullptr, 0, &dl.attributes_len);
		if (ret == SQL_NO_DATA) {
			break;
		}

		if (dl.description_len == -1 || dl.attributes_len == -1) {
			std::string diag = Diagnostics::Read(env, SQL_HANDLE_ENV);
			throw ScannerException("'SQLDriversW' length failed, return: " + std::to_string(ret) + ", diagnostics: '" +
			                       diag + "'");
		}

		res.emplace_back(std::move(dl));
	}

	return res;
}

static std::vector<Driver> ReadDrivers() {
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

	int max_attempts = 4;

	for (int attempt = 0; attempt < max_attempts; attempt++) {
		std::vector<DriverLength> driver_lengths = ReadDriverLengths(env.get());
		std::vector<Driver> drivers;
		drivers.reserve(driver_lengths.size());

		bool first = true;
		bool length_mismatch = false;

		for (DriverLength &dl : driver_lengths) {
			std::vector<SQLWCHAR> desc_buf;
			desc_buf.resize(dl.description_len + 1);
			SQLSMALLINT desc_len = 0;
			std::vector<SQLWCHAR> attr_buf;
			attr_buf.resize(dl.attributes_len + 1);
			SQLSMALLINT attr_len = 0;

			{
				SQLRETURN ret = SQLDriversW(env.get(), first ? SQL_FETCH_FIRST : SQL_FETCH_NEXT, desc_buf.data(),
				                            static_cast<SQLSMALLINT>(desc_buf.size()), &desc_len, attr_buf.data(),
				                            static_cast<SQLSMALLINT>(attr_buf.size()), &attr_len);
				if (first) {
					first = false;
				}
				if (ret == SQL_NO_DATA) {
					length_mismatch = true;
					break;
				}
				if (!SQL_SUCCEEDED(ret)) {
					std::string diag = Diagnostics::Read(env.get(), SQL_HANDLE_ENV);
					throw ScannerException("'SQLDriversW' failed, return: " + std::to_string(ret) + ", diagnostics: '" +
					                       diag + "'");
				}
				if (dl.description_len != desc_len) {
					length_mismatch = true;
					break;
				}
				if (dl.attributes_len != attr_len) {
					length_mismatch = true;
					break;
				}
				std::string desc = WideChar::Narrow(desc_buf.data(), desc_buf.size() - 1);
				std::string attrs_str = WideChar::Narrow(attr_buf.data(), attr_buf.size() - 1);
				auto attrs = ParseAttributes(attrs_str);
				Driver drv(std::move(desc), std::move(attrs));
				drivers.emplace_back(std::move(drv));
			}
		}

		if (!length_mismatch) {
			return drivers;
		}
	}

	throw ScannerException(
	    "'odbc_list_drivers' error: unable to read drivers list due to drivers/descriptions/attributes size mismatch "
	    "that can be caused by concurrent modifications of the registered drivers, attempts: " +
	    std::to_string(max_attempts));
}

static void SetAttributesMap(idx_t row_idx, Vector &attributes_vector, const std::vector<DriverAttribute> &attributes) {
	Vector struct_vector = attributes_vector.GetChild(0);
	list_entry_t le;
	le.offset = struct_vector.GetSize();
	le.length = attributes.size();
	list_entry_t *list_data = attributes_vector.GetDataMutable<list_entry_t>();
	list_data[row_idx] = le;

	// reserves the space in the child vector
	struct_vector.SetSize(le.offset + le.length);

	Vector names_vector = struct_vector.GetChild(0);
	Vector values_vector = struct_vector.GetChild(1);
	names_vector.SetSize(le.offset + le.length);
	values_vector.SetSize(le.offset + le.length);
	for (size_t i = 0; i < le.length; i++) {
		idx_t idx = i + le.offset;
		const DriverAttribute &att = attributes.at(i);
		names_vector.AssignStringUnsafe(idx, std::string_view(att.name.c_str(), att.name.length()));
		values_vector.AssignStringUnsafe(idx, std::string_view(att.value.c_str(), att.value.length()));
	}
}

static void ListDrivers(TableFunction::ExecInput &input, DataChunk &output) {
	LocalInitData &ldata = input.GetLocalState<LocalInitData>();

	if (ldata.state == ExecState::EXHAUSTED) {
		output.GetVector(0).SetSize(0);
		return;
	}

	if (ldata.state == ExecState::UNINITIALIZED) {
		ldata.drivers = ReadDrivers();
		ldata.state = ExecState::EXECUTED;
	}

	Vector description_vector = output.GetVector(0);
	Vector attributes_vector = output.GetVector(1);

	if (!description_vector || !attributes_vector) {
		throw ScannerException("'odbc_list_drivers' error: invalid null output vector");
	}

	idx_t vector_size = output.GetCapacity();
	idx_t row_idx = 0;
	for (; row_idx < vector_size; row_idx++) {
		if (ldata.driver_idx >= ldata.drivers.size()) {
			ldata.state = ExecState::EXHAUSTED;
			break;
		}
		const Driver &driver = ldata.drivers.at(ldata.driver_idx++);
		description_vector.AssignStringUnsafe(
		    row_idx, std::string_view(driver.description.c_str(), driver.description.length()));
		SetAttributesMap(row_idx, attributes_vector, driver.attributes);
	}
	output.GetVector(0).SetSize(row_idx);
}

void OdbcListDriversFunction::Register(Extension &extension, Factory &factory) {
	auto fun = TableFunction::Create(factory);
	fun.SetName("odbc_list_drivers");

	// callbacks
	fun.SetBindCallback(odbc_list_drivers_bind);
	fun.SetInitGlobalCallback(odbc_list_drivers_init);
	fun.SetInitLocalCallback(odbc_list_drivers_local_init);
	fun.SetExecCallback(odbc_list_drivers_function);

	// register and cleanup
	try {
		extension.Register(fun);
	} catch (std::exception &e) {
		throw ScannerException(std::string("\'") + "odbc_list_drivers" +
		                       "\' function registration failed: " + e.what());
	}
}

} // namespace odbcscanner

static void odbc_list_drivers_bind(duckdb::cxx::TableFunction::BindInput &input) {
	try {
		odbcscanner::Bind(input);
	} catch (std::exception &e) {
		throw odbcscanner::FunctionArgs::BinderError(e.what());
	}
}

static void odbc_list_drivers_init(duckdb::cxx::TableFunction::InitGlobalInput &input) {
	odbcscanner::GlobalInit(input);
}

static void odbc_list_drivers_local_init(duckdb::cxx::TableFunction::InitLocalInput &input) {
	odbcscanner::LocalInit(input);
}

static void odbc_list_drivers_function(duckdb::cxx::TableFunction::ExecInput &input) {
	duckdb::cxx::DataChunk output = input.GetOutputChunk();
	odbcscanner::ListDrivers(input, output);
}
