#include "types.hpp"

#include <cstring>

#include "capi_pointers.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

static const std::string trunc_diag_code("01004");

template <>
std::pair<std::string, bool> Types::ExtractFunctionArg<std::string>(duckdb_data_chunk chunk, idx_t col_idx) {
	idx_t col_count = duckdb_data_chunk_get_column_count(chunk);
	if (col_idx >= col_count) {
		throw ScannerException("Cannot extract VARCHAR function argument: column not found, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	duckdb_vector vec = duckdb_data_chunk_get_vector(chunk, col_idx);
	if (vec == nullptr) {
		throw ScannerException("Cannot extract VARCHAR function argument: vector is NULL, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	idx_t rows_count = duckdb_data_chunk_get_size(chunk);
	if (rows_count == 0) {
		throw ScannerException("Cannot extract VARCHAR function argument: vector contains no rows, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	uint64_t *validity = duckdb_vector_get_validity(vec);
	if (validity != nullptr && !duckdb_validity_row_is_valid(validity, 0)) {
		return std::make_pair("", true);
	}

	duckdb_string_t *str_data = reinterpret_cast<duckdb_string_t *>(duckdb_vector_get_data(vec));
	duckdb_string_t str_t = str_data[0];

	const char *cstr = duckdb_string_t_data(&str_t);
	uint32_t len = duckdb_string_t_length(str_t);
	std::string res(cstr, len);

	return std::make_pair(std::move(res), false);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<std::string>(DbmsQuirks &, duckdb_vector vec) {
	duckdb_string_t *data = reinterpret_cast<duckdb_string_t *>(duckdb_vector_get_data(vec));
	duckdb_string_t dstr = data[0];
	const char *cstr = duckdb_string_t_data(&dstr);
	uint32_t len = duckdb_string_t_length(dstr);
	return ScannerValue(cstr, len);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<std::string>(DbmsQuirks &, duckdb_value value) {
	auto str_ptr = VarcharPtr(duckdb_get_varchar(value), VarcharDeleter);
	return ScannerValue(str_ptr.get());
}

template <>
void TypeSpecific::BindOdbcParam<std::string>(QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = SQL_WVARCHAR;
	WideString &wstring = param.Value<WideString>();
	size_t len_bytes = wstring.length<size_t>() * sizeof(SQLWCHAR);
	if (len_bytes > ctx.quirks.var_len_params_long_threshold_bytes) {
		sqltype = SQL_WLONGVARCHAR;
	}
	SQLRETURN ret =
	    SQLBindParameter(ctx.hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_WCHAR, sqltype, wstring.length<SQLULEN>(), 0,
	                     reinterpret_cast<SQLPOINTER>(wstring.data()), param.LengthBytes(), &param.LengthBytes());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' VARCHAR failed, expected type: " + std::to_string(sqltype) +
		                       ", value: '" + param.ToUtf8String(1 << 10) + "', index: " + std::to_string(param_idx) +
		                       ", query: '" + ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" +
		                       diag + "'");
	}
}

static std::pair<std::string, bool> FetchTail(QueryContext &ctx, SQLSMALLINT col_idx, std::vector<SQLWCHAR> &buf,
                                              SQLLEN len_bytes) {
	if (len_bytes % sizeof(SQLWCHAR) != 0) {
		len_bytes -= 1;
	}
	size_t len = static_cast<size_t>(len_bytes / sizeof(SQLWCHAR));

	size_t head_size = buf.size() - 1;
	buf.resize(len + 1);

	SQLWCHAR *buf_tail_ptr = buf.data() + head_size;
	size_t buf_tail_size = buf.size() - head_size;
	SQLLEN len_tail_bytes = 0;

	SQLRETURN ret_tail = SQLGetData(ctx.hstmt, col_idx, SQL_C_WCHAR, buf_tail_ptr,
	                                static_cast<SQLLEN>(buf_tail_size * sizeof(SQLWCHAR)), &len_tail_bytes);
	if (!SQL_SUCCEEDED(ret_tail)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' tail for VARCHAR failed, column index: " + std::to_string(col_idx) +
		                       ", query: '" + ctx.query + "', return: " + std::to_string(ret_tail) +
		                       ", diagnostics: '" + diag + "'");
	}

	if (len_tail_bytes % sizeof(SQLWCHAR) != 0) {
		len_tail_bytes -= 1;
	}
	size_t len_tail = static_cast<size_t>(len_tail_bytes / sizeof(SQLWCHAR));
	size_t len_tail_expected = len - head_size;
	if (len_tail != len_tail_expected) {
		throw ScannerException(
		    "'SQLGetData' for VARCHAR failed due to inconsistent tail length reported by the driver, column "
		    "index: " +
		    std::to_string(col_idx) + ", query: '" + ctx.query + "', return: " + std::to_string(ret_tail) +
		    ", head length: " + std::to_string(len_tail_expected) + ", actual: " + std::to_string(len_tail));
	}

	std::string str = WideChar::Narrow(buf.data(), buf.size() - 1);
	return std::make_pair(std::move(str), false);
}

static std::pair<std::string, bool> FetchMultiReads(QueryContext &ctx, SQLSMALLINT col_idx,
                                                    std::vector<SQLWCHAR> &buf) {
	for (size_t i = 1; true; i++) {
		size_t prev_len = buf.size();
		size_t prev_written = prev_len - 1;
		buf.resize(prev_len * 2);
		SQLLEN len_bytes = 0;

		SQLWCHAR *buf_ptr = buf.data() + prev_written;
		size_t buf_size = buf.size() - prev_written;
		SQLRETURN ret = SQLGetData(ctx.hstmt, col_idx, SQL_C_WCHAR, buf_ptr,
		                           static_cast<SQLLEN>(buf_size * sizeof(SQLWCHAR)), &len_bytes);

		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
			throw ScannerException("'SQLGetData' multi for VARCHAR failed, read number: " + std::to_string(i) +
			                       " column index: " + std::to_string(col_idx) + ", query: '" + ctx.query +
			                       "', return: " + std::to_string(ret) +
			                       ", prev written: " + std::to_string(prev_written) + ", diagnostics: '" + diag + "'");
		}

		std::string diag_code;
		if (ret == SQL_SUCCESS_WITH_INFO) {
			diag_code = Diagnostics::ReadCode(ctx.hstmt, SQL_HANDLE_STMT);
		}

		if (ret == SQL_SUCCESS || diag_code != trunc_diag_code) {
			if (len_bytes % sizeof(SQLWCHAR) != 0) {
				len_bytes -= 1;
			}

			size_t buf_size_full = prev_written + (len_bytes / sizeof(SQLWCHAR));
			std::string str = WideChar::Narrow(buf.data(), buf_size_full);
			return std::make_pair(std::move(str), false);
		}
	}
}

static std::pair<std::string, bool> FetchSinglePart(QueryContext &ctx, SQLSMALLINT col_idx, std::vector<SQLWCHAR> &buf,
                                                    SQLLEN len_bytes) {
	if (len_bytes % sizeof(SQLWCHAR) != 0) {
		len_bytes -= 1;
	}
	size_t len = static_cast<size_t>(len_bytes / sizeof(SQLWCHAR));
	buf.resize(len + 1);
	SQLLEN len_read_bytes = 0;

	SQLRETURN ret_tail = SQLGetData(ctx.hstmt, col_idx, SQL_C_WCHAR, buf.data(),
	                                static_cast<SQLLEN>(buf.size() * sizeof(SQLWCHAR)), &len_read_bytes);
	if (!SQL_SUCCEEDED(ret_tail)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException(
		    "'SQLGetData' single part for VARCHAR tail failed, column index: " + std::to_string(col_idx) +
		    ", query: '" + ctx.query + "', return: " + std::to_string(ret_tail) + ", diagnostics: '" + diag + "'");
	}

	if (len_read_bytes % sizeof(SQLWCHAR) != 0) {
		len_read_bytes -= 1;
	}
	if (len_read_bytes != len_bytes) {
		throw ScannerException(
		    "'SQLGetData' single part for VARCHAR failed due to inconsistent part length reported by the driver, "
		    "column "
		    "index: " +
		    std::to_string(col_idx) + ", query: '" + ctx.query + "', return: " + std::to_string(ret_tail) +
		    ", first read length: " + std::to_string(len_bytes) + ", actual: " + std::to_string(len_read_bytes));
	}

	std::string str = WideChar::Narrow(buf.data(), buf.size() - 1);
	return std::make_pair(std::move(str), false);
}

static std::pair<std::string, bool> FetchInternal(QueryContext &ctx, SQLSMALLINT col_idx) {
	std::vector<SQLWCHAR> buf;
	buf.resize(4096);
	SQLLEN len_bytes = 0;
	SQLRETURN ret = SQLGetData(ctx.hstmt, col_idx, SQL_C_WCHAR, buf.data(),
	                           static_cast<SQLLEN>(buf.size() * sizeof(SQLWCHAR)), &len_bytes);

	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' for VARCHAR failed, column index: " + std::to_string(col_idx) +
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
			return std::make_pair("", true);
		}

		if (len_bytes % sizeof(SQLWCHAR) != 0) {
			len_bytes -= 1;
		}

		size_t len = len_bytes / sizeof(SQLWCHAR);
		std::string str = WideChar::Narrow(buf.data(), len);
		return std::make_pair(std::move(str), false);
	}

	// invariant: ret = SQL_SUCCESS_WITH_INFO && diag_code == "01004"

	if (ctx.quirks.var_len_data_single_part) {
		return FetchSinglePart(ctx, col_idx, buf, len_bytes);
	}

	if (len_bytes != SQL_NO_TOTAL) {
		return FetchTail(ctx, col_idx, buf, len_bytes);
	}

	return FetchMultiReads(ctx, col_idx, buf);
}

template <>
void TypeSpecific::FetchAndSetResult<std::string>(QueryContext &ctx, OdbcType &, SQLSMALLINT col_idx, duckdb_vector vec,
                                                  idx_t row_idx) {
	auto fetched = FetchInternal(ctx, col_idx);

	if (fetched.second) {
		Types::SetNullValueToResult(vec, row_idx);
		return;
	}

	duckdb_vector_assign_string_element_len(vec, row_idx, fetched.first.c_str(), fetched.first.length());
}

template <>
duckdb_type TypeSpecific::ResolveColumnType<std::string>(QueryContext &, ResultColumn &) {
	return DUCKDB_TYPE_VARCHAR;
}

} // namespace odbcscanner
