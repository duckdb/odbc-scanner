#include "odbc_scanner.hpp"

#include <cstdint>
#include <string>

#include "capi_pointers.hpp"
#include "defer.hpp"
#include "diagnostics.hpp"
#include "registries.hpp"
#include "scanner_exception.hpp"
#include "types.hpp"

DUCKDB_EXTENSION_EXTERN

static void odbc_begin_transaction_function(duckdb_function_info info, duckdb_data_chunk input,
                                            duckdb_vector output) noexcept;

namespace odbcscanner {

static void BeginTransaction(duckdb_function_info info, duckdb_data_chunk input, duckdb_vector output) {
	(void)info;

	auto conn_arg = Types::ExtractFunctionArg<int64_t>(input, 0);
	if (conn_arg.second) {
		throw ScannerException("'odbc_begin_transaction' error: specified ODBC connection must be not NULL");
	}
	int64_t conn_id = conn_arg.first;
	auto conn_ptr = ConnectionsRegistry::Remove(conn_id);

	if (conn_ptr.get() == nullptr) {
		throw ScannerException("'odbc_begin_transaction' error: open ODBC connection not found, id: " +
		                       std::to_string(conn_id));
	}

	// Return the connection to registry at the end of the block
	auto deferred_conn = Defer([&conn_ptr] { ConnectionsRegistry::Add(std::move(conn_ptr)); });

	OdbcConnection &conn = *conn_ptr;

	SQLRETURN ret = SQLSetConnectAttr(conn.dbc, SQL_ATTR_AUTOCOMMIT, static_cast<SQLPOINTER>(SQL_AUTOCOMMIT_OFF), 0);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(conn.dbc, SQL_HANDLE_DBC);
		throw ScannerException("'SQLSetConnectAttr' failed for SQL_AUTOCOMMIT_OFF, return: " + std::to_string(ret) +
		                       ", diagnostics: '" + diag + "'");
	}

	duckdb_vector_ensure_validity_writable(output);
	uint64_t *result_validity = duckdb_vector_get_validity(output);
	duckdb_validity_set_row_invalid(result_validity, 0);
}

void OdbcBeginTransactionFunction::Register(duckdb_connection conn) {
	auto fun = ScalarFunctionPtr(duckdb_create_scalar_function(), ScalarFunctionDeleter);
	duckdb_scalar_function_set_name(fun.get(), "odbc_begin_transaction");

	// parameters and return
	auto bigint_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_BIGINT), LogicalTypeDeleter);
	auto varchar_type = LogicalTypePtr(duckdb_create_logical_type(DUCKDB_TYPE_VARCHAR), LogicalTypeDeleter);
	duckdb_scalar_function_add_parameter(fun.get(), bigint_type.get());
	duckdb_scalar_function_set_return_type(fun.get(), varchar_type.get());

	// callbacks
	duckdb_scalar_function_set_function(fun.get(), odbc_begin_transaction_function);

	// options
	duckdb_scalar_function_set_volatile(fun.get());
	duckdb_scalar_function_set_special_handling(fun.get());

	// register and cleanup
	duckdb_state state = duckdb_register_scalar_function(conn, fun.get());

	if (state != DuckDBSuccess) {
		throw ScannerException("'odbc_begin_transaction' function registration failed");
	}
}

} // namespace odbcscanner

static void odbc_begin_transaction_function(duckdb_function_info info, duckdb_data_chunk input,
                                            duckdb_vector output) noexcept {
	try {
		odbcscanner::BeginTransaction(info, input, output);
	} catch (std::exception &e) {
		duckdb_scalar_function_set_error(info, e.what());
	}
}
