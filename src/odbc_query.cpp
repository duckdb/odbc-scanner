#include "capi_odbc_scanner.h"

#include <memory>
#include <sql.h>
#include <sqlext.h>
#include <string>

DUCKDB_EXTENSION_EXTERN

// todo: error handling

struct QueryContext {
	SQLHANDLE env = nullptr;
	SQLHANDLE dbc = nullptr;
	HSTMT hstmt = SQL_NULL_HSTMT;

	// todo: enum
	bool finished = false;

	QueryContext() {
		SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
		SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(static_cast<uintptr_t>(SQL_OV_ODBC3)),
		              0);
		SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
		// todo: handle URL bind param
		std::string connect_str = "Driver={DuckDB Driver};threads=1;";
		SQLDriverConnect(dbc, nullptr, reinterpret_cast<SQLCHAR *>(const_cast<char *>(connect_str.c_str())), SQL_NTS,
		                 nullptr, 0, nullptr, SQL_DRIVER_COMPLETE_REQUIRED);
		SQLAllocHandle(SQL_HANDLE_STMT, dbc, &hstmt);
	}

	QueryContext &operator=(const QueryContext &) = delete;
	QueryContext &operator=(QueryContext &&other);

	~QueryContext() {
		SQLFreeStmt(hstmt, SQL_CLOSE);
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
		SQLFreeHandle(SQL_HANDLE_ENV, env);
		SQLDisconnect(dbc);
		SQLFreeHandle(SQL_HANDLE_DBC, dbc);
	}

	static void Destroy(void *ctx_in) noexcept {
		auto ctx = reinterpret_cast<QueryContext *>(ctx_in);
		delete ctx;
	}
};

struct BindData {
	std::string url;
	std::string query;

	BindData(std::string url_in, std::string query_in) : url(std::move(url_in)), query(std::move(query_in)) {
	}

	BindData &operator=(const BindData &) = delete;
	BindData &operator=(BindData &&other);

	static void Destroy(void *bdata_in) noexcept {
		auto bdata = reinterpret_cast<BindData *>(bdata_in);
		delete bdata;
	}
};

void odbc_query_bind(duckdb_bind_info info) noexcept {
	duckdb_logical_type type = duckdb_create_logical_type(DUCKDB_TYPE_BIGINT);
	duckdb_bind_add_result_column(info, "forty_two", type);
	duckdb_destroy_logical_type(&type);

	duckdb_value url_val = duckdb_bind_get_parameter(info, 0);
	char *url_ptr = duckdb_get_varchar(url_val);
	std::string url(url_ptr);
	duckdb_destroy_value(&url_val);

	duckdb_value query_val = duckdb_bind_get_parameter(info, 1);
	char *query_ptr = duckdb_get_varchar(query_val);
	std::string query(query_ptr);
	duckdb_destroy_value(&query_val);

	auto bdata_ptr = std::unique_ptr<BindData>(new BindData(std::move(url), std::move(query)));
	duckdb_bind_set_bind_data(info, bdata_ptr.release(), BindData::Destroy);
}

void odbc_query_init(duckdb_init_info info) noexcept {
	(void)info;
}

void odbc_query_local_init(duckdb_init_info info) noexcept {
	auto ctx_ptr = std::unique_ptr<QueryContext>(new QueryContext());
	duckdb_init_set_init_data(info, ctx_ptr.release(), QueryContext::Destroy);
}

void odbc_query_function(duckdb_function_info info, duckdb_data_chunk output) noexcept {
	BindData &bdata = *reinterpret_cast<BindData *>(duckdb_function_get_bind_data(info));
	QueryContext &ctx = *reinterpret_cast<QueryContext *>(duckdb_function_get_local_init_data(info));

	if (ctx.finished) {
		duckdb_data_chunk_set_size(output, 0);
		return;
	}

	SQLExecDirect(ctx.hstmt, reinterpret_cast<SQLCHAR *>(const_cast<char *>(bdata.query.c_str())), SQL_NTS);

	duckdb_vector vec = duckdb_data_chunk_get_vector(output, 0);
	int64_t *vec_data = reinterpret_cast<int64_t *>(duckdb_vector_get_data(vec));

	idx_t row_idx = 0;
	for (; row_idx < duckdb_vector_size(); row_idx++) {
		SQLRETURN status = SQLFetch(ctx.hstmt);
		if (status != SQL_SUCCESS) {
			ctx.finished = true;
			break;
		}

		int32_t fetched = 0;
		SQLGetData(ctx.hstmt, 1, SQL_C_SLONG, &fetched, sizeof(fetched), nullptr);

		vec_data[row_idx] = fetched;
	}

	duckdb_data_chunk_set_size(output, row_idx);
}

void odbc_query_register(duckdb_connection connection) /* noexcept */ {
	duckdb_table_function function = duckdb_create_table_function();
	duckdb_table_function_set_name(function, "odbc_query");

	// string parameters
	duckdb_logical_type type = duckdb_create_logical_type(DUCKDB_TYPE_VARCHAR);
	duckdb_table_function_add_parameter(function, type);
	duckdb_table_function_add_parameter(function, type);
	duckdb_destroy_logical_type(&type);

	// callbacks
	duckdb_table_function_set_bind(function, odbc_query_bind);
	duckdb_table_function_set_init(function, odbc_query_init);
	duckdb_table_function_set_local_init(function, odbc_query_local_init);
	duckdb_table_function_set_function(function, odbc_query_function);

	// register and cleanup
	duckdb_register_table_function(connection, function);
	duckdb_destroy_table_function(&function);
}
