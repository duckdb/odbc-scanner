#include "types.hpp"

#include <cstring>

#include "binary.hpp"
#include "capi_pointers.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

// Differences from fetching type_varchar: 1. no wide, 2. no nul-termination

static const std::string trunc_diag_code("01004");

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<duckdb_blob>(DbmsQuirks &, duckdb_vector vec) {
	duckdb_string_t *data = reinterpret_cast<duckdb_string_t *>(duckdb_vector_get_data(vec));
	duckdb_string_t dstr = data[0];
	const char *buf_ptr = duckdb_string_t_data(&dstr);
	uint32_t len = duckdb_string_t_length(dstr);
	std::vector<char> buf;
	buf.resize(len);
	std::memcpy(buf.data(), buf_ptr, len);
	ScannerBlob blob(std::move(buf));
	return ScannerValue(std::move(blob));
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<duckdb_blob>(DbmsQuirks &, duckdb_value value) {
	duckdb_blob dblob = duckdb_get_blob(value);
	std::vector<char> buf;
	buf.resize(dblob.size);
	std::memcpy(buf.data(), dblob.data, dblob.size);
	ScannerBlob blob(std::move(buf));
	return ScannerValue(std::move(blob));
}

template <>
void TypeSpecific::BindOdbcParam<duckdb_blob>(QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = SQL_VARBINARY;
	ScannerBlob &blob = param.Value<ScannerBlob>();
	if (blob.size<uint32_t>() > ctx.quirks.var_len_params_long_threshold_bytes) {
		sqltype = SQL_LONGVARBINARY;
	}
	SQLRETURN ret =
	    SQLBindParameter(ctx.hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_BINARY, sqltype, blob.size<SQLULEN>(), 0,
	                     reinterpret_cast<SQLPOINTER>(blob.data()), param.LengthBytes(), &param.LengthBytes());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' VARBINARY failed, expected type: " + std::to_string(sqltype) +
		                       "', index: " + std::to_string(param_idx) + ", query: '" + ctx.query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

static void FetchTail(QueryContext &ctx, SQLSMALLINT col_idx, std::vector<SQLCHAR> &buf, SQLLEN len_bytes) {
	size_t head_size = buf.size();
	buf.resize(len_bytes);

	SQLCHAR *buf_tail_ptr = buf.data() + head_size;
	size_t buf_tail_size = buf.size() - head_size;
	SQLLEN len_tail_bytes = 0;

	SQLRETURN ret_tail =
	    SQLGetData(ctx.hstmt, col_idx, SQL_C_BINARY, buf_tail_ptr, static_cast<SQLLEN>(buf_tail_size), &len_tail_bytes);
	if (!SQL_SUCCEEDED(ret_tail)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' tail for VARBINARY failed, column index: " + std::to_string(col_idx) +
		                       ", query: '" + ctx.query + "', return: " + std::to_string(ret_tail) +
		                       ", diagnostics: '" + diag + "'");
	}

	SQLLEN len_tail_expected = len_bytes - head_size;
	if (len_tail_bytes != len_tail_expected) {
		throw ScannerException(
		    "'SQLGetData' for VARBINARY failed due to inconsistent tail length reported by the driver, column "
		    "index: " +
		    std::to_string(col_idx) + ", query: '" + ctx.query + "', return: " + std::to_string(ret_tail) +
		    ", head length: " + std::to_string(head_size) + ", tail length expected: " +
		    std::to_string(len_tail_expected) + ", actual: " + std::to_string(len_tail_bytes));
	}
}

static void FetchMultiReads(QueryContext &ctx, SQLSMALLINT col_idx, std::vector<SQLCHAR> &buf) {
	for (size_t i = 1; true; i++) {
		size_t prev_len = buf.size();
		buf.resize(prev_len * 2);
		SQLLEN len_bytes = 0;

		SQLCHAR *buf_ptr = buf.data() + prev_len;
		size_t buf_size = buf.size() - prev_len;
		SQLRETURN ret =
		    SQLGetData(ctx.hstmt, col_idx, SQL_C_BINARY, buf_ptr, static_cast<SQLLEN>(buf_size), &len_bytes);

		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
			throw ScannerException("'SQLGetData' multi for VARBINARY failed, read number: " + std::to_string(i) +
			                       " column index: " + std::to_string(col_idx) + ", query: '" + ctx.query +
			                       "', return: " + std::to_string(ret) + ", prev written: " + std::to_string(prev_len) +
			                       ", diagnostics: '" + diag + "'");
		}

		std::string diag_code;
		if (ret == SQL_SUCCESS_WITH_INFO) {
			diag_code = Diagnostics::ReadCode(ctx.hstmt, SQL_HANDLE_STMT);
		}

		if (ret == SQL_SUCCESS || diag_code != trunc_diag_code) {
			size_t buf_size_full = prev_len + len_bytes;
			buf.resize(buf_size_full);
			return;
		}
	}
}

static void FetchSinglePart(QueryContext &ctx, SQLSMALLINT col_idx, std::vector<SQLCHAR> &buf, SQLLEN len_bytes) {
	buf.resize(len_bytes);
	SQLLEN len_read_bytes = 0;

	SQLRETURN ret_tail =
	    SQLGetData(ctx.hstmt, col_idx, SQL_C_BINARY, buf.data(), static_cast<SQLLEN>(buf.size()), &len_read_bytes);
	if (!SQL_SUCCEEDED(ret_tail)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException(
		    "'SQLGetData' single part for VARBINARY tail failed, column index: " + std::to_string(col_idx) +
		    ", query: '" + ctx.query + "', return: " + std::to_string(ret_tail) + ", diagnostics: '" + diag + "'");
	}

	if (len_read_bytes != len_bytes) {
		throw ScannerException(
		    "'SQLGetData' single part for VARBIANRY failed due to inconsistent part length reported by the driver, "
		    "column "
		    "index: " +
		    std::to_string(col_idx) + ", query: '" + ctx.query + "', return: " + std::to_string(ret_tail) +
		    ", first read length: " + std::to_string(len_bytes) + ", actual: " + std::to_string(len_read_bytes));
	}
}

static std::pair<std::vector<SQLCHAR>, bool> FetchInternal(QueryContext &ctx, SQLSMALLINT col_idx) {
	std::vector<SQLCHAR> buf;
	buf.resize(8192);
	SQLLEN len_bytes = 0;
	SQLRETURN ret =
	    SQLGetData(ctx.hstmt, col_idx, SQL_C_BINARY, buf.data(), static_cast<SQLLEN>(buf.size()), &len_bytes);

	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' for VARBINARY failed, column index: " + std::to_string(col_idx) +
		                       ", query: '" + ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" +
		                       diag + "'");
	}

	std::string diag_code;
	if (ret == SQL_SUCCESS_WITH_INFO) {
		diag_code = Diagnostics::ReadCode(ctx.hstmt, SQL_HANDLE_STMT);
	}

	// single read
	if (ret == SQL_SUCCESS || diag_code != trunc_diag_code) {

		if (len_bytes == SQL_NULL_DATA) {
			return std::make_pair(std::vector<SQLCHAR>(), true);
		}

		buf.resize(len_bytes);
		return std::make_pair(std::move(buf), false);
	}

	// invariant: ret = SQL_SUCCESS_WITH_INFO && diag_code == "01004"

	if (ctx.quirks.var_len_data_single_part) {
		FetchSinglePart(ctx, col_idx, buf, len_bytes);
		return std::make_pair(std::move(buf), false);
	}

	if (len_bytes != SQL_NO_TOTAL) {
		FetchTail(ctx, col_idx, buf, len_bytes);
		return std::make_pair(std::move(buf), false);
	}

	FetchMultiReads(ctx, col_idx, buf);
	return std::make_pair(std::move(buf), false);
}

template <>
void TypeSpecific::FetchAndSetResult<duckdb_blob>(QueryContext &ctx, OdbcType &, SQLSMALLINT col_idx, duckdb_vector vec,
                                                  idx_t row_idx) {
	auto fetched = FetchInternal(ctx, col_idx);

	if (fetched.second) {
		Types::SetNullValueToResult(vec, row_idx);
		return;
	}

	duckdb_vector_assign_string_element_len(vec, row_idx, reinterpret_cast<char *>(fetched.first.data()),
	                                        fetched.first.size());
}

template <>
duckdb_type TypeSpecific::ResolveColumnType<duckdb_blob>(QueryContext &, ResultColumn &) {
	return DUCKDB_TYPE_BLOB;
}

} // namespace odbcscanner