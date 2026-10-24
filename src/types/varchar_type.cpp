#include "types.hpp"

#include <cstring>

#include "function_args.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"

namespace odbcscanner {

static const std::string trunc_diag_code("01004");

template <>
std::pair<std::string, bool> Types::ExtractFunctionArg<std::string>(ScalarFunction::ExecInput &input, idx_t col_idx) {
	idx_t col_count = input.GetArgCount();
	if (col_idx >= col_count) {
		throw ScannerException("Cannot extract VARCHAR function argument: column not found, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	Vector vec = input.GetArg(col_idx);
	if (!vec) {
		throw ScannerException("Cannot extract VARCHAR function argument: vector is NULL, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	idx_t rows_count = input.GetRowCount();
	if (rows_count == 0) {
		throw ScannerException("Cannot extract VARCHAR function argument: vector contains no rows, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	VectorView view = vec.GetView();
	idx_t row_idx = view.SelAt(0);
	if (!view.RowIsValid(row_idx)) {
		return std::make_pair("", true);
	}

	auto vec_type = vec.GetLogicalType();
	if (!vec_type) {
		throw ScannerException("Cannot extract VARCHAR function argument: column type is NULL, column: " +
		                       std::to_string(col_idx) + ", columns count: " + std::to_string(col_count));
	}

	LogicalTypeId vec_type_id = Types::TypeIdOf(vec_type);
	if (vec_type_id != LogicalTypeId::VARCHAR) {
		throw ScannerException("Cannot extract VARCHAR function argument: invalid column type: " +
		                       std::to_string(static_cast<uint32_t>(vec_type_id)) +
		                       ", expected: " + std::to_string(static_cast<uint32_t>(LogicalTypeId::VARCHAR)) +
		                       ", column: " + std::to_string(col_idx) +
		                       ", columns count: " + std::to_string(col_count));
	}

	const varchar_t *str_data = reinterpret_cast<const varchar_t *>(view.data);
	const varchar_t &str_t = str_data[row_idx];

	const char *cstr = str_t.data();
	uint32_t len = str_t.size();
	std::string res(cstr, len);

	return std::make_pair(std::move(res), false);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<std::string>(const DbmsQuirks &, Vector &vec, idx_t row_idx) {
	varchar_t *data = vec.GetDataMutable<varchar_t>();
	varchar_t &dstr = data[row_idx];
	const char *cstr = dstr.data();
	uint32_t len = dstr.size();
	return ScannerValue(cstr, len);
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<std::string>(const DbmsQuirks &, const Value *value) {
	std::string str = std::string(value->Get<varchar_t>().view());
	return ScannerValue(str.c_str());
}

template <>
void TypeSpecific::BindOdbcParam<std::string>(const QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = SQL_WVARCHAR;
	WideString &wstring = param.Value<WideString>();
	size_t len_bytes = wstring.length<size_t>() * sizeof(SQLWCHAR);
	if (len_bytes > ctx.quirks.var_len_params_long_threshold_bytes) {
		sqltype = SQL_WLONGVARCHAR;
	}
	SQLRETURN ret =
	    SQLBindParameter(ctx.hstmt(), param_idx, SQL_PARAM_INPUT, SQL_C_WCHAR, sqltype, wstring.length<SQLULEN>(), 0,
	                     reinterpret_cast<SQLPOINTER>(wstring.data()), param.LengthBytes(), &param.LengthBytes());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' VARCHAR failed, expected type: " + std::to_string(sqltype) +
		                       ", value: '" + param.ToUtf8String(1 << 10) + "', index: " + std::to_string(param_idx) +
		                       ", query: '" + ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" +
		                       diag + "'");
	}
}

static std::pair<std::string, bool> FetchTail(const QueryContext &ctx, SQLSMALLINT col_idx, std::vector<SQLWCHAR> &buf,
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

	SQLRETURN ret_tail = SQLGetData(ctx.hstmt(), col_idx, SQL_C_WCHAR, buf_tail_ptr,
	                                static_cast<SQLLEN>(buf_tail_size * sizeof(SQLWCHAR)), &len_tail_bytes);
	if (!SQL_SUCCEEDED(ret_tail)) {
		std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
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

static std::pair<std::string, bool> FetchMultiReads(const QueryContext &ctx, SQLSMALLINT col_idx,
                                                    std::vector<SQLWCHAR> &buf) {
	for (size_t i = 1; true; i++) {
		size_t prev_len = buf.size();
		size_t prev_written = prev_len - 1;
		buf.resize(prev_len * 2);
		SQLLEN len_bytes = 0;

		SQLWCHAR *buf_ptr = buf.data() + prev_written;
		size_t buf_size = buf.size() - prev_written;
		SQLRETURN ret = SQLGetData(ctx.hstmt(), col_idx, SQL_C_WCHAR, buf_ptr,
		                           static_cast<SQLLEN>(buf_size * sizeof(SQLWCHAR)), &len_bytes);

		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
			throw ScannerException("'SQLGetData' multi for VARCHAR failed, read number: " + std::to_string(i) +
			                       " column index: " + std::to_string(col_idx) + ", query: '" + ctx.query +
			                       "', return: " + std::to_string(ret) +
			                       ", prev written: " + std::to_string(prev_written) + ", diagnostics: '" + diag + "'");
		}

		std::string diag_code;
		if (ret == SQL_SUCCESS_WITH_INFO) {
			diag_code = Diagnostics::ReadCode(ctx.hstmt(), SQL_HANDLE_STMT);
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

static std::pair<std::string, bool> FetchSinglePart(const QueryContext &ctx, SQLSMALLINT col_idx,
                                                    std::vector<SQLWCHAR> &buf, SQLLEN len_bytes) {
	if (len_bytes % sizeof(SQLWCHAR) != 0) {
		len_bytes -= 1;
	}
	size_t len = static_cast<size_t>(len_bytes / sizeof(SQLWCHAR));
	buf.resize(len + 1);
	SQLLEN len_read_bytes = 0;

	SQLRETURN ret_tail = SQLGetData(ctx.hstmt(), col_idx, SQL_C_WCHAR, buf.data(),
	                                static_cast<SQLLEN>(buf.size() * sizeof(SQLWCHAR)), &len_read_bytes);
	if (!SQL_SUCCEEDED(ret_tail)) {
		std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
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

static std::pair<std::string, bool> FetchInternal(const QueryContext &ctx, SQLSMALLINT col_idx) {
	std::vector<SQLWCHAR> buf;
	buf.resize(4096);
	SQLLEN len_bytes = 0;
	SQLRETURN ret = SQLGetData(ctx.hstmt(), col_idx, SQL_C_WCHAR, buf.data(),
	                           static_cast<SQLLEN>(buf.size() * sizeof(SQLWCHAR)), &len_bytes);

	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' for VARCHAR failed, column index: " + std::to_string(col_idx) +
		                       ", query: '" + ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" +
		                       diag + "'");
	}

	std::string diag_code;
	if (ret == SQL_SUCCESS_WITH_INFO) {
		diag_code = Diagnostics::ReadCode(ctx.hstmt(), SQL_HANDLE_STMT);
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
void TypeSpecific::FetchAndSetResult<std::string>(const QueryContext &ctx, const OdbcType &, SQLSMALLINT col_idx,
                                                  Vector &vec, idx_t row_idx) {
	auto fetched = FetchInternal(ctx, col_idx);

	if (fetched.second) {
		Types::SetNullValueToResult(vec, row_idx);
		return;
	}

	vec.AssignStringUnsafe(row_idx, std::string_view(fetched.first.c_str(), fetched.first.length()));
}

template <>
LogicalTypeId TypeSpecific::ResolveColumnType<std::string>(const QueryContext &, ResultColumn &) {
	return LogicalTypeId::VARCHAR;
}

} // namespace odbcscanner
