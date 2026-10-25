#include "odbc_scanner.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "function_args.hpp"
#include "connection.hpp"
#include "diagnostics.hpp"
#include "make_unique.hpp"
#include "odbc_api.hpp"
#include "registries.hpp"
#include "scanner_exception.hpp"
#include "strings.hpp"
#include "types.hpp"

static void odbc_connect_bind(duckdb::cxx::ScalarFunction::BindInput &input);

namespace odbcscanner {

static void AppendUsernameAndPassword(ScalarFunction::ExecInput &input, std::string &conn_str) {
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
static std::string ExtractAccessToken(ScalarFunction::ExecInput &input, idx_t arg_idx) {
	auto pair = Types::ExtractFunctionArg<std::string>(input, arg_idx);
	if (pair.second) {
		// NULL value - treat as "no token"
		return "";
	}
	return pair.first;
}

static bool HasNulls(const VectorView &view, idx_t attrs_offset, idx_t attrs_count) {
	for (idx_t i = attrs_offset; i < attrs_offset + attrs_count; i++) {
		if (!view.IsValid(i)) {
			return true;
		}
	}
	return false;
}

static std::pair<std::vector<OdbcConnectionAttribute>, bool> ExtractAttrs(ScalarFunction::ExecInput &input,
                                                                          idx_t args_count) {
	if (args_count <= 1) {
		return std::make_pair(std::vector<OdbcConnectionAttribute>(), false);
	}
	Vector vec = input.GetArg(args_count - 1);
	if (!vec) {
		return std::make_pair(std::vector<OdbcConnectionAttribute>(), false);
	}
	auto vec_type = vec.GetLogicalType();
	LogicalTypeId type_id = Types::TypeIdOf(vec_type);
	if (type_id != LogicalTypeId::MAP) {
		return std::make_pair(std::vector<OdbcConnectionAttribute>(), false);
	}

	{
		auto key_type = vec_type.GetMapKeyType();
		LogicalTypeId key_type_id = Types::TypeIdOf(key_type);
		if (key_type_id != LogicalTypeId::INTEGER) {
			throw ScannerException("'odbc_connect' error: specified connection attribute map keys must be INTEGER");
		}
	}

	auto value_type = vec_type.GetMapValueType();
	LogicalTypeId value_type_id = Types::TypeIdOf(value_type);
	if (value_type_id != LogicalTypeId::INTEGER && value_type_id != LogicalTypeId::BIGINT) {
		throw ScannerException(
		    "'odbc_connect' error: specified connection attribute map values must be INTEGER or BIGINT");
	}

	// map entries of the first row
	VectorView map_view = vec.GetView();
	const list_entry_t *map_entries = map_view.Data<list_entry_t>();
	const list_entry_t &map_entry = map_entries[map_view.SelAt(0)];
	idx_t attrs_offset = map_entry.offset;
	idx_t attrs_count = map_entry.length;

	Vector struct_vec = vec.GetChild(0);
	VectorView struct_view = struct_vec.GetView();
	Vector key_vec = struct_vec.GetChild(0);
	VectorView keys_view = key_vec.GetView();
	Vector value_vec = struct_vec.GetChild(1);
	VectorView values_view = value_vec.GetView();

	if (HasNulls(struct_view, attrs_offset, attrs_count) || HasNulls(keys_view, attrs_offset, attrs_count) ||
	    HasNulls(values_view, attrs_offset, attrs_count)) {
		throw ScannerException("'odbc_connect' error: specified connection attribute map entries must not be NULL");
	}

	std::vector<OdbcConnectionAttribute> attrs;
	attrs.reserve(attrs_count);
	int32_t *keys = key_vec.GetDataMutable<int32_t>();
	for (idx_t i = attrs_offset; i < attrs_offset + attrs_count; i++) {
		int32_t key = keys[i];
		int64_t val = 0;
		if (value_type_id == LogicalTypeId::INTEGER) {
			int32_t *values = value_vec.GetDataMutable<int32_t>();
			val = static_cast<int64_t>(values[i]);
		} else if (value_type_id == LogicalTypeId::BIGINT) {
			int64_t *values = value_vec.GetDataMutable<int64_t>();
			val = values[i];
		}
		attrs.emplace_back(OdbcConnectionAttribute(key, val));
	}

	return std::make_pair(std::move(attrs), true);
}

static void Bind(ScalarFunction::BindInput &input) {
	OdbcConnection::VerifyExternalAccess(input.GetContext());
}

static void Connect(ScalarFunction::ExecInput &input) {
	Vector output = input.GetResult();

	idx_t args_count = input.GetArgCount();
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

	auto oc_ptr = std_make_unique<OdbcConnection>(conn_str, access_token, attrs.first);

	int64_t *result_data = output.GetDataMutable<int64_t>();
	result_data[0] = ConnectionsRegistry::Add(std::move(oc_ptr));
}

void OdbcConnectFunction::Register(Extension &extension, Factory &factory) {
	auto fun = ScalarFunction::Create(factory);
	fun.SetName("odbc_connect");
	FunctionSignature sig = fun.GetSignature();

	// parameters and return
	auto any_type = factory.CreateType(LogicalTypeId::ANY);
	auto bigint_type = factory.CreateType(LogicalTypeId::BIGINT);
	sig.AddArgs("args", any_type);
	sig.SetReturnType(bigint_type);

	// callbacks
	fun.SetBindCallback(odbc_connect_bind);
	fun.SetExecCallback(Connect);

	// options
	fun.SetStability(duckdb::cxx::FunctionStability::VOLATILE);
	fun.SetNullHandling(duckdb::cxx::FunctionNullHandling::SPECIAL);

	// register and cleanup
	try {
		extension.Register(fun);
	} catch (std::exception &e) {
		throw ScannerException(std::string("\'") + "odbc_connect" + "\' function registration failed: " + e.what());
	}
}

} // namespace odbcscanner

static void odbc_connect_bind(duckdb::cxx::ScalarFunction::BindInput &input) {
	try {
		odbcscanner::Bind(input);
	} catch (std::exception &e) {
		throw odbcscanner::FunctionArgs::BinderError(e.what());
	}
}
