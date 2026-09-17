#include "odbc_scanner.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "capi_pointers.hpp"
#include "connection.hpp"
#include "diagnostics.hpp"
#include "make_unique.hpp"
#include "odbc_api.hpp"
#include "registries.hpp"
#include "scanner_exception.hpp"
#include "strings.hpp"
#include "types.hpp"

DUCKDB_EXTENSION_EXTERN

static const std::string ODBCSCANNER_DEBUG_CONN_STRING_ENV_VAR = "ODBCSCANNER_DEBUG_CONN_STRING_ENV_VAR";

static void odbc_connect_function(duckdb_function_info info, duckdb_data_chunk input, duckdb_vector output) noexcept;

namespace odbcscanner {

static void AppendUsernameAndPassword(duckdb_data_chunk input, std::string &conn_str) {
	auto username_pair = Types::ExtractFunctionArg<std::string>(input, 1);
	if (username_pair.second) {
		throw ScannerException("'odbc_connect' error: specified username argument must be not NULL");
	}
	std::string username = username_pair.first;

	auto password_pair = Types::ExtractFunctionArg<std::string>(input, 2);
	if (password_pair.second) {
		throw ScannerException("'odbc_connect' error: specified password argument must be not NULL");
	}
	std::string password = password_pair.first;

	std::string conn_str_upper = Strings::ToUpper(conn_str);
	if (conn_str_upper.find("UID") != std::string::npos) {
		throw ScannerException("'odbc_connect' error: username (UID) cannot be specified in both connection string and "
		                       "a separate argument");
	}
	if (conn_str_upper.find("PWD") != std::string::npos) {
		throw ScannerException("'odbc_connect' error: password (PWD) cannot be specified in both connection string and "
		                       "a separate argument");
	}

	if (conn_str.length() == 0 || conn_str.at(conn_str.length() - 1) != ';') {
		conn_str.append(";");
	}
	conn_str.append("UID=");
	conn_str.append(username);
	conn_str.append(";");

	conn_str.append("PWD=");
	conn_str.append(password);
	conn_str.append(";");
}

// Extract an optional access_token string from the given argument index.
// Returns an empty string when the argument is absent or NULL.
static std::string ExtractAccessToken(duckdb_data_chunk input, idx_t arg_idx) {
	auto pair = Types::ExtractFunctionArg<std::string>(input, arg_idx);
	if (pair.second) {
		// NULL value - treat as "no token"
		return "";
	}
	return pair.first;
}

static bool HasNulls(uint64_t *validity, idx_t attrs_count) {
	if (validity == nullptr) {
		return false;
	}
	for (idx_t i = 0; i < attrs_count; i++) {
		if (!duckdb_validity_row_is_valid(validity, i)) {
			return true;
		}
	}
	return false;
}

static std::pair<std::vector<OdbcConnectionAttribute>, bool> ExtractAttrs(duckdb_data_chunk input, idx_t args_count) {
	if (args_count <= 1) {
		return std::make_pair(std::vector<OdbcConnectionAttribute>(), false);
	}
	duckdb_vector vec = duckdb_data_chunk_get_vector(input, args_count - 1);
	if (!vec) {
		return std::make_pair(std::vector<OdbcConnectionAttribute>(), false);
	}
	auto vec_type = LogicalTypePtr(duckdb_vector_get_column_type(vec), LogicalTypeDeleter);
	duckdb_type type_id = duckdb_get_type_id(vec_type.get());
	if (type_id != DUCKDB_TYPE_MAP) {
		return std::make_pair(std::vector<OdbcConnectionAttribute>(), false);
	}

	{
		auto key_type = LogicalTypePtr(duckdb_map_type_key_type(vec_type.get()), LogicalTypeDeleter);
		duckdb_type key_type_id = duckdb_get_type_id(key_type.get());
		if (key_type_id != DUCKDB_TYPE_INTEGER) {
			throw ScannerException("'odbc_connect' error: specified connection attribute map keys must be INTEGER");
		}
	}

	auto value_type = LogicalTypePtr(duckdb_map_type_value_type(vec_type.get()), LogicalTypeDeleter);
	duckdb_type value_type_id = duckdb_get_type_id(value_type.get());
	if (value_type_id != DUCKDB_TYPE_INTEGER && value_type_id != DUCKDB_TYPE_BIGINT) {
		throw ScannerException(
		    "'odbc_connect' error: specified connection attribute map values must be INTEGER or BIGINT");
	}

	duckdb_vector struct_vec = duckdb_list_vector_get_child(vec);
	uint64_t *struct_validity = duckdb_vector_get_validity(struct_vec);
	duckdb_vector key_vec = duckdb_struct_vector_get_child(struct_vec, 0);
	uint64_t *keys_validity = duckdb_vector_get_validity(key_vec);
	duckdb_vector value_vec = duckdb_struct_vector_get_child(struct_vec, 1);
	uint64_t *values_validity = duckdb_vector_get_validity(value_vec);
	idx_t attrs_count = duckdb_list_vector_get_size(vec);

	if (HasNulls(struct_validity, attrs_count) || HasNulls(keys_validity, attrs_count) ||
	    HasNulls(values_validity, attrs_count)) {
		throw ScannerException("'odbc_connect' error: specified connection attribute map entries must not be NULL");
	}

	std::vector<OdbcConnectionAttribute> attrs;
	attrs.reserve(attrs_count);
	int32_t *keys = reinterpret_cast<int32_t *>(duckdb_vector_get_data(key_vec));
	for (idx_t i = 0; i < attrs_count; i++) {
		int32_t key = keys[i];
		int64_t val = 0;
		if (value_type_id == DUCKDB_TYPE_INTEGER) {
			int32_t *values = reinterpret_cast<int32_t *>(duckdb_vector_get_data(value_vec));
			val = static_cast<int64_t>(values[i]);
		} else if (value_type_id == DUCKDB_TYPE_BIGINT) {
			int64_t *values = reinterpret_cast<int64_t *>(duckdb_vector_get_data(value_vec));
			val = values[i];
		}
		attrs.emplace_back(OdbcConnectionAttribute(key, val));
	}

	return std::make_pair(std::move(attrs), true);
}

static void Connect(duckdb_function_info info, duckdb_data_chunk input, duckdb_vector output) {
	(void)info;

	idx_t args_count = duckdb_data_chunk_get_column_count(input);
	auto attrs = ExtractAttrs(input, args_count);
	idx_t varchar_args_count = attrs.second ? args_count - 1 : args_count;

	if (varchar_args_count < 1 || varchar_args_count > 3) {
		throw ScannerException(
		    "'odbc_connect' error: invalid number of arguments specified, count: " + std::to_string(args_count) +
		    ", supported signatures:'n"
		    " odbc_connect(conn_string VARCHAR, [conn_attributes MAP(INTEGER -> BIGINT)]),\n"
		    " odbc_connect(conn_string VARCHAR, access_token VARCHAR, [conn_attributes MAP(INTEGER -> BIGINT)]),\n"
		    " odbc_connect(conn_string VARCHAR, username VARCHAR, password VARCHAR), [conn_attributes MAP(INTEGER -> "
		    "BIGINT)])");
	}

	auto conn_str_pair = Types::ExtractFunctionArg<std::string>(input, 0);
	if (conn_str_pair.second) {
		throw ScannerException("'odbc_connect' error: specified connection string argument must be not NULL");
	}
	std::string conn_str = conn_str_pair.first;

	std::string access_token;

	if (varchar_args_count == 2) {
		// Signature: (conn_string, access_token)
		access_token = ExtractAccessToken(input, 1);
	} else if (varchar_args_count == 3) {
		// Signature: (conn_string, username, password)
		AppendUsernameAndPassword(input, conn_str);
	}

	// Env var fetch is not thread-safe, should be used only for debugging,
	// ideally this logic should be moved into SQLLogic test runner.
	if (conn_str.rfind(ODBCSCANNER_DEBUG_CONN_STRING_ENV_VAR, 0) == 0 && conn_str.find(";") == std::string::npos) {
		std::vector<std::string> parts = Strings::Split(conn_str, '=');
		if (parts.size() == 2 && ODBCSCANNER_DEBUG_CONN_STRING_ENV_VAR == parts.at(0)) {
			std::string &var_name = parts.at(1);
			char *var = std::getenv(var_name.c_str());
			conn_str = var != nullptr ? std::string(var) : "Driver={DuckDB Driver};";
		}
	}

	auto oc_ptr = std_make_unique<OdbcConnection>(conn_str, access_token, attrs.first);

	int64_t *result_data = reinterpret_cast<int64_t *>(duckdb_vector_get_data(output));
	result_data[0] = ConnectionsRegistry::Add(std::move(oc_ptr));
}

void OdbcConnectFunction::Register(duckdb_connection conn) {
	auto fun = ScalarFunctionPtr(duckdb_create_scalar_function(), ScalarFunctionDeleter);
	duckdb_scalar_function_set_name(fun.get(), "odbc_connect");

	// parameters and return
	auto any_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_ANY), LogicalTypeDeleter);
	auto bigint_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_BIGINT), LogicalTypeDeleter);
	duckdb_scalar_function_set_varargs(fun.get(), any_type.get());
	duckdb_scalar_function_set_return_type(fun.get(), bigint_type.get());

	// callbacks
	duckdb_scalar_function_set_function(fun.get(), odbc_connect_function);

	// options
	duckdb_scalar_function_set_volatile(fun.get());
	duckdb_scalar_function_set_special_handling(fun.get());

	// register and cleanup
	duckdb_state state = duckdb_register_scalar_function(conn, fun.get());

	if (state != DuckDBSuccess) {
		throw ScannerException("'odbc_connect' function registration failed");
	}
}

} // namespace odbcscanner

static void odbc_connect_function(duckdb_function_info info, duckdb_data_chunk input, duckdb_vector output) noexcept {
	try {
		odbcscanner::Connect(info, input, output);
	} catch (std::exception &e) {
		duckdb_scalar_function_set_error(info, e.what());
	}
}
