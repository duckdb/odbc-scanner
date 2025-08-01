#include "capi_odbc_scanner.h"
#include "common.hpp"
#include "odbc_connection.hpp"
#include "scanner_exception.hpp"

#include <memory>
#include <sql.h>
#include <sqlext.h>
#include <string>
#include <vector>

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

struct BindData {
	OdbcConnection &conn;
	std::string query;
	HSTMT hstmt = SQL_NULL_HSTMT;

	BindData(OdbcConnection &conn_in, std::string query_in, HSTMT hstmt_in)
	    : conn(conn_in), query(std::move(query_in)), hstmt(hstmt_in) {
	}

	BindData &operator=(const BindData &) = delete;
	BindData &operator=(BindData &&other) = delete;

	~BindData() {
		SQLFreeStmt(hstmt, SQL_CLOSE);
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	}

	static void Destroy(void *bdata_in) noexcept {
		auto bdata = reinterpret_cast<BindData *>(bdata_in);
		delete bdata;
	}
};

struct LocalInitData {
	OdbcConnection &conn;

	// todo: enum
	bool finished = false;

	LocalInitData(OdbcConnection &conn_in) : conn(conn_in) {
	}

	LocalInitData &operator=(const LocalInitData &) = delete;
	LocalInitData &operator=(LocalInitData &&other);

	static void Destroy(void *ctx_in) noexcept {
		auto ctx = reinterpret_cast<LocalInitData *>(ctx_in);
		delete ctx;
	}
};

static void Bind(duckdb_bind_info info) {
	duckdb_value conn_ptr_val = duckdb_bind_get_parameter(info, 0);
	int64_t conn_ptr_num = duckdb_get_int64(conn_ptr_val);
	OdbcConnection *conn_ptr = reinterpret_cast<OdbcConnection *>(conn_ptr_num);
	OdbcConnection &conn = *conn_ptr;
	duckdb_destroy_value(&conn_ptr_val);

	duckdb_value query_val = duckdb_bind_get_parameter(info, 1);
	char *query_ptr = duckdb_get_varchar(query_val);
	std::string query(query_ptr);
	duckdb_destroy_value(&query_val);

	HSTMT hstmt = SQL_NULL_HSTMT;
	SQLAllocHandle(SQL_HANDLE_STMT, conn.dbc, &hstmt);
	SQLPrepare(hstmt, reinterpret_cast<SQLCHAR *>(const_cast<char *>(query.c_str())), SQL_NTS);

	SQLSMALLINT cols_count = 0;
	SQLNumResultCols(hstmt, &cols_count);

	for (SQLSMALLINT i = 0; i < cols_count; i++) {
		SQLLEN ctype = -1;
		SQLColAttribute(hstmt, i + 1, SQL_DESC_CONCISE_TYPE, nullptr, 0, nullptr, &ctype);

		duckdb_logical_type lt = nullptr;
		switch (ctype) {
		case SQL_INTEGER: {
			lt = duckdb_create_logical_type(DUCKDB_TYPE_BIGINT);
			break;
		}
		case SQL_VARCHAR: {
			lt = duckdb_create_logical_type(DUCKDB_TYPE_VARCHAR);
			break;
		}
		default:
			// todo
			return;
		}

		std::vector<SQLCHAR> buf;
		SQLSMALLINT len = 0;
		buf.resize(1024);
		SQLColAttribute(hstmt, i + 1, SQL_DESC_NAME, buf.data(), static_cast<SQLSMALLINT>(buf.size()), &len, nullptr);
		std::string name(reinterpret_cast<char *>(buf.data()), len);

		duckdb_bind_add_result_column(info, name.c_str(), lt);
		duckdb_destroy_logical_type(&lt);
	}

	auto bdata_ptr = std::unique_ptr<BindData>(new BindData(conn, std::move(query), hstmt));
	duckdb_bind_set_bind_data(info, bdata_ptr.release(), BindData::Destroy);
}

static void LocalInit(duckdb_init_info info) {
	BindData &bdata = *reinterpret_cast<BindData *>(duckdb_init_get_bind_data(info));
	auto ctx_ptr = std::unique_ptr<LocalInitData>(new LocalInitData(bdata.conn));
	duckdb_init_set_init_data(info, ctx_ptr.release(), LocalInitData::Destroy);
}

static void Query(duckdb_function_info info, duckdb_data_chunk output) {
	BindData &bdata = *reinterpret_cast<BindData *>(duckdb_function_get_bind_data(info));
	LocalInitData &ctx = *reinterpret_cast<LocalInitData *>(duckdb_function_get_local_init_data(info));

	if (ctx.finished) {
		duckdb_data_chunk_set_size(output, 0);
		return;
	}

	SQLExecute(bdata.hstmt);

	SQLSMALLINT cols_count = 0;
	SQLNumResultCols(bdata.hstmt, &cols_count);

	idx_t row_idx = 0;
	for (; row_idx < duckdb_vector_size(); row_idx++) {

		SQLRETURN status = SQLFetch(bdata.hstmt);
		if (status != SQL_SUCCESS) {
			ctx.finished = true;
			break;
		}

		for (idx_t col_idx = 0; col_idx < static_cast<idx_t>(cols_count); col_idx++) {
			duckdb_vector vec = duckdb_data_chunk_get_vector(output, col_idx);
			SQLLEN ctype = -1;
			SQLColAttribute(bdata.hstmt, static_cast<SQLSMALLINT>(col_idx + 1), SQL_DESC_CONCISE_TYPE, nullptr, 0,
			                nullptr, &ctype);
			switch (ctype) {
			case SQL_INTEGER: {
				int64_t *vec_data = reinterpret_cast<int64_t *>(duckdb_vector_get_data(vec));
				int32_t fetched = 0;
				SQLGetData(bdata.hstmt, static_cast<SQLSMALLINT>(col_idx + 1), SQL_C_SLONG, &fetched, sizeof(fetched),
				           nullptr);
				vec_data[row_idx] = fetched;
				break;
			}
			case SQL_VARCHAR: {
				// todo: parts
				std::vector<SQLCHAR> buf;
				buf.resize(1024);
				SQLLEN len = 0;
				SQLGetData(bdata.hstmt, col_idx + 1, SQL_C_CHAR, buf.data(), static_cast<SQLSMALLINT>(buf.size()),
				           &len);
				std::string str(reinterpret_cast<char *>(buf.data()), len);
				duckdb_vector_assign_string_element_len(vec, row_idx, str.c_str(), str.length());
				break;
			}
			default:
				// todo
				break;
			}
		}
	}
	duckdb_data_chunk_set_size(output, row_idx);
}

} // namespace odbcscanner

void odbc_query_bind(duckdb_bind_info info) noexcept {
	try {
		odbcscanner::Bind(info);
	} catch (std::exception &e) {
		duckdb_bind_set_error(info, e.what());
	}
}

void odbc_query_init(duckdb_init_info info) noexcept {
	(void)info;
}

void odbc_query_local_init(duckdb_init_info info) noexcept {
	try {
		odbcscanner::LocalInit(info);
	} catch (std::exception &e) {
		duckdb_init_set_error(info, e.what());
	}
}

void odbc_query_function(duckdb_function_info info, duckdb_data_chunk output) noexcept {
	try {
		odbcscanner::Query(info, output);
	} catch (std::exception &e) {
		duckdb_function_set_error(info, e.what());
	}
}

duckdb_state odbc_query_register(duckdb_connection conn) /* noexcept */ {
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
	duckdb_state state = duckdb_register_table_function(conn, fun);
	duckdb_destroy_table_function(&fun);

	return state;
}
