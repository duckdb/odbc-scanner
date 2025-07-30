#include "capi_odbc_scanner.h"
#include "odbc_connection.hpp"

#include <memory>
#include <sql.h>
#include <sqlext.h>
#include <string>

DUCKDB_EXTENSION_EXTERN

// todo: error handling

struct QueryContext {
	OdbcConnection &conn;
	HSTMT hstmt = SQL_NULL_HSTMT;

	// todo: enum
	bool finished = false;

	QueryContext(OdbcConnection &conn_in) : conn(conn_in) {
		SQLAllocHandle(SQL_HANDLE_STMT, conn.dbc, &hstmt);
	}

	QueryContext &operator=(const QueryContext &) = delete;
	QueryContext &operator=(QueryContext &&other);

	~QueryContext() {
		SQLFreeStmt(hstmt, SQL_CLOSE);
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	}

	static void Destroy(void *ctx_in) noexcept {
		auto ctx = reinterpret_cast<QueryContext *>(ctx_in);
		delete ctx;
	}
};

struct BindData {
	OdbcConnection &conn;
	std::string query;

	BindData(OdbcConnection &conn_in, std::string query_in) : conn(conn_in), query(std::move(query_in)) {
	}

	BindData &operator=(const BindData &) = delete;
	BindData &operator=(BindData &&other) = delete;

	static void Destroy(void *bdata_in) noexcept {
		auto bdata = reinterpret_cast<BindData *>(bdata_in);
		delete bdata;
	}
};

void odbc_query_bind(duckdb_bind_info info) noexcept {
	duckdb_logical_type type = duckdb_create_logical_type(DUCKDB_TYPE_BIGINT);
	duckdb_bind_add_result_column(info, "forty_two", type);
	duckdb_destroy_logical_type(&type);

	duckdb_value conn_ptr_val = duckdb_bind_get_parameter(info, 0);
	int64_t conn_ptr_num = duckdb_get_int64(conn_ptr_val);
	OdbcConnection *conn_ptr = reinterpret_cast<OdbcConnection *>(conn_ptr_num);
	OdbcConnection &conn = *conn_ptr;
	duckdb_destroy_value(&conn_ptr_val);

	duckdb_value query_val = duckdb_bind_get_parameter(info, 1);
	char *query_ptr = duckdb_get_varchar(query_val);
	std::string query(query_ptr);
	duckdb_destroy_value(&query_val);

	auto bdata_ptr = std::unique_ptr<BindData>(new BindData(conn, std::move(query)));
	duckdb_bind_set_bind_data(info, bdata_ptr.release(), BindData::Destroy);
}

void odbc_query_init(duckdb_init_info info) noexcept {
	(void)info;
}

void odbc_query_local_init(duckdb_init_info info) noexcept {
	BindData &bdata = *reinterpret_cast<BindData *>(duckdb_init_get_bind_data(info));
	auto ctx_ptr = std::unique_ptr<QueryContext>(new QueryContext(bdata.conn));
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

void odbc_query_register(duckdb_connection conn) /* noexcept */ {
	duckdb_table_function fun = duckdb_create_table_function();
	duckdb_table_function_set_name(fun, "odbc_query");

	// parameters
	duckdb_logical_type bigint_type = duckdb_create_logical_type(DUCKDB_TYPE_BIGINT);
	duckdb_logical_type varchar_type = duckdb_create_logical_type(DUCKDB_TYPE_VARCHAR);
	duckdb_table_function_add_parameter(fun, bigint_type);
	duckdb_table_function_add_parameter(fun, varchar_type);
	duckdb_destroy_logical_type(&varchar_type);
	duckdb_destroy_logical_type(&bigint_type);

	// callbacks
	duckdb_table_function_set_bind(fun, odbc_query_bind);
	duckdb_table_function_set_init(fun, odbc_query_init);
	duckdb_table_function_set_local_init(fun, odbc_query_local_init);
	duckdb_table_function_set_function(fun, odbc_query_function);

	// register and cleanup
	duckdb_register_table_function(conn, fun);
	duckdb_destroy_table_function(&fun);
}
