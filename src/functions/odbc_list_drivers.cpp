#include "odbc_scanner.hpp"

#include <string>
#include <vector>

#include "capi_pointers.hpp"
#include "diagnostics.hpp"
#include "make_unique.hpp"
#include "odbc_api.hpp"
#include "scanner_exception.hpp"
#include "strings.hpp"
#include "widechar.hpp"

DUCKDB_EXTENSION_EXTERN

static void odbc_list_drivers_bind(duckdb_bind_info info) noexcept;
static void odbc_list_drivers_init(duckdb_init_info info) noexcept;
static void odbc_list_drivers_local_init(duckdb_init_info info) noexcept;
static void odbc_list_drivers_function(duckdb_function_info info, duckdb_data_chunk output) noexcept;

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

	static void Destroy(void *ldata_in) noexcept {
		auto ldata = reinterpret_cast<LocalInitData *>(ldata_in);
		delete ldata;
	}
};

} // namespace

static void Bind(duckdb_bind_info info) {
	auto varchar_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_VARCHAR), LogicalTypeDeleter);
	auto map_type = LogicalTypePtr(duckdb_create_map_type(varchar_type.get(), varchar_type.get()), LogicalTypeDeleter);

	duckdb_bind_add_result_column(info, "description", varchar_type.get());
	duckdb_bind_add_result_column(info, "attributes", map_type.get());
}

static void GlobalInit(duckdb_init_info info) {
	(void)info;
	// no-op
}

static void LocalInit(duckdb_init_info info) {
	auto ldata_ptr = std_make_unique<LocalInitData>();
	duckdb_init_set_init_data(info, ldata_ptr.release(), LocalInitData::Destroy);
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

static void SetAttributesMap(idx_t row_idx, duckdb_vector attributes_vector,
                             const std::vector<DriverAttribute> &attributes) {
	duckdb_vector struct_vector = duckdb_list_vector_get_child(attributes_vector);
	duckdb_list_entry le;
	le.offset = duckdb_list_vector_get_size(attributes_vector);
	le.length = attributes.size();
	duckdb_list_entry *list_data = reinterpret_cast<duckdb_list_entry *>(duckdb_vector_get_data(attributes_vector));
	list_data[row_idx] = le;

	duckdb_state reserve_st = duckdb_list_vector_reserve(attributes_vector, le.offset + le.length);
	if (reserve_st != DuckDBSuccess) {
		throw ScannerException("'odbc_list_drivers' error: 'duckdb_list_vector_reserve' failed, offset: " +
		                       std::to_string(le.offset) + ", length: " + std::to_string(le.length));
	}
	duckdb_state set_size_state = duckdb_list_vector_set_size(attributes_vector, le.offset + le.length);
	if (set_size_state != DuckDBSuccess) {
		throw ScannerException("'odbc_list_drivers' error: 'duckdb_list_vector_set_size' failed, offset: " +
		                       std::to_string(le.offset) + ", length: " + std::to_string(le.length));
	}

	duckdb_vector names_vector = duckdb_struct_vector_get_child(struct_vector, 0);
	duckdb_vector values_vector = duckdb_struct_vector_get_child(struct_vector, 1);
	for (size_t i = 0; i < le.length; i++) {
		idx_t idx = i + le.offset;
		const DriverAttribute &att = attributes.at(i);
		duckdb_vector_assign_string_element_len(names_vector, idx, att.name.c_str(), att.name.length());
		duckdb_vector_assign_string_element_len(values_vector, idx, att.value.c_str(), att.value.length());
	}
}

static void ListDrivers(duckdb_function_info info, duckdb_data_chunk output) {
	LocalInitData &ldata = *reinterpret_cast<LocalInitData *>(duckdb_function_get_local_init_data(info));

	if (ldata.state == ExecState::EXHAUSTED) {
		duckdb_data_chunk_set_size(output, 0);
		return;
	}

	if (ldata.state == ExecState::UNINITIALIZED) {
		ldata.drivers = ReadDrivers();
		ldata.state = ExecState::EXECUTED;
	}

	duckdb_vector description_vector = duckdb_data_chunk_get_vector(output, 0);
	duckdb_vector attributes_vector = duckdb_data_chunk_get_vector(output, 1);

	if (description_vector == nullptr || attributes_vector == nullptr) {
		throw ScannerException("'odbc_list_drivers' error: invalid null output vector");
	}

	idx_t row_idx = 0;
	for (; row_idx < duckdb_vector_size(); row_idx++) {
		if (ldata.driver_idx >= ldata.drivers.size()) {
			ldata.state = ExecState::EXHAUSTED;
			break;
		}
		const Driver &driver = ldata.drivers.at(ldata.driver_idx++);
		duckdb_vector_assign_string_element_len(description_vector, row_idx, driver.description.c_str(),
		                                        driver.description.length());
		SetAttributesMap(row_idx, attributes_vector, driver.attributes);
	}
	duckdb_data_chunk_set_size(output, row_idx);
}

void OdbcListDriversFunction::Register(duckdb_connection conn) {
	auto fun = TableFunctionPtr(duckdb_create_table_function(), TableFunctionDeleter);
	duckdb_table_function_set_name(fun.get(), "odbc_list_drivers");

	// callbacks
	duckdb_table_function_set_bind(fun.get(), odbc_list_drivers_bind);
	duckdb_table_function_set_init(fun.get(), odbc_list_drivers_init);
	duckdb_table_function_set_local_init(fun.get(), odbc_list_drivers_local_init);
	duckdb_table_function_set_function(fun.get(), odbc_list_drivers_function);

	// register and cleanup
	duckdb_state state = duckdb_register_table_function(conn, fun.get());

	if (state != DuckDBSuccess) {
		throw ScannerException("'odbc_list_drivers' function registration failed");
	}
}

} // namespace odbcscanner

static void odbc_list_drivers_bind(duckdb_bind_info info) noexcept {
	try {
		odbcscanner::Bind(info);
	} catch (std::exception &e) {
		duckdb_bind_set_error(info, e.what());
	}
}

static void odbc_list_drivers_init(duckdb_init_info info) noexcept {
	try {
		odbcscanner::GlobalInit(info);
	} catch (std::exception &e) {
		duckdb_init_set_error(info, e.what());
	}
}

static void odbc_list_drivers_local_init(duckdb_init_info info) noexcept {
	try {
		odbcscanner::LocalInit(info);
	} catch (std::exception &e) {
		duckdb_init_set_error(info, e.what());
	}
}

static void odbc_list_drivers_function(duckdb_function_info info, duckdb_data_chunk output) noexcept {
	try {
		odbcscanner::ListDrivers(info, output);
	} catch (std::exception &e) {
		duckdb_function_set_error(info, e.what());
	}
}
