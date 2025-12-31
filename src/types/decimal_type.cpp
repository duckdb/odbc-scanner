#include "types.hpp"

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <limits>

#include "capi_pointers.hpp"
#include "connection.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<duckdb_decimal>(DbmsQuirks &quirks, duckdb_vector vec) {
	auto ltype = LogicalTypePtr(duckdb_vector_get_column_type(vec), LogicalTypeDeleter);

	duckdb_decimal dec;
	std::memset(&dec, '\0', sizeof(dec));
	dec.width = duckdb_decimal_width(ltype.get());
	dec.scale = duckdb_decimal_scale(ltype.get());

	duckdb_type type_id = duckdb_decimal_internal_type(ltype.get());
	switch (type_id) {
	case DUCKDB_TYPE_SMALLINT: {
		int16_t *data = reinterpret_cast<int16_t *>(duckdb_vector_get_data(vec));
		int16_t val = data[0];
		dec.value.lower = static_cast<uint64_t>(val);
		dec.value.upper = val >= 0 ? 0 : -1;
		break;
	}
	case DUCKDB_TYPE_INTEGER: {
		int32_t *data = reinterpret_cast<int32_t *>(duckdb_vector_get_data(vec));
		int32_t val = data[0];
		dec.value.lower = static_cast<uint64_t>(val);
		dec.value.upper = val >= 0 ? 0 : -1;
		break;
	}
	case DUCKDB_TYPE_BIGINT: {
		int64_t *data = reinterpret_cast<int64_t *>(duckdb_vector_get_data(vec));
		int64_t val = data[0];
		dec.value.lower = static_cast<uint64_t>(val);
		dec.value.upper = val >= 0 ? 0 : -1;
		break;
	}
	case DUCKDB_TYPE_HUGEINT: {
		duckdb_hugeint *data = reinterpret_cast<duckdb_hugeint *>(duckdb_vector_get_data(vec));
		dec.value = data[0];
		break;
	}
	default:
		throw ScannerException("Invalid unsupported DECIMAL internal type, ID: " + std::to_string(type_id));
	}

	return ScannerParam(dec, quirks.decimal_params_as_chars);
}

template <>
ScannerParam TypeSpecific::ExtractNotNullParam<duckdb_decimal>(DbmsQuirks &quirks, duckdb_value value) {
	duckdb_decimal val = duckdb_get_decimal(value);
	return ScannerParam(val, quirks.decimal_params_as_chars);
}

template <>
void TypeSpecific::BindOdbcParam<SQL_NUMERIC_STRUCT>(QueryContext &ctx, ScannerParam &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = param.ExpectedType() != SQL_PARAM_TYPE_UNKNOWN ? param.ExpectedType() : SQL_NUMERIC;
	SQL_NUMERIC_STRUCT &ns = param.Value<SQL_NUMERIC_STRUCT>();
	SQLRETURN ret = SQLBindParameter(ctx.hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_NUMERIC, sqltype,
	                                 static_cast<SQLULEN>(ns.precision), static_cast<SQLSMALLINT>(ns.scale),
	                                 reinterpret_cast<SQLPOINTER>(&ns), param.LengthBytes(), &param.LengthBytes());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' failed, type: " + std::to_string(sqltype) +
		                       ", index: " + std::to_string(param_idx) + ", query: '" + ctx.query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

template <>
void TypeSpecific::BindOdbcParam<DecimalChars>(QueryContext &ctx, ScannerParam &param, SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = param.ExpectedType() != SQL_PARAM_TYPE_UNKNOWN ? param.ExpectedType() : SQL_NUMERIC;
	DecimalChars &dc = param.Value<DecimalChars>();
	SQLRETURN ret =
	    SQLBindParameter(ctx.hstmt, param_idx, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, param.LengthBytes(), 0,
	                     reinterpret_cast<SQLPOINTER>(dc.data()), param.LengthBytes(), &param.LengthBytes());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' failed, type: " + std::to_string(sqltype) +
		                       ", index: " + std::to_string(param_idx) + ", query: '" + ctx.query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

static void SetDescriptorFields(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx) {
	SQLHDESC hdesc = nullptr;
	{
		SQLRETURN ret = SQLGetStmtAttr(ctx.hstmt, SQL_ATTR_APP_ROW_DESC, &hdesc, 0, NULL);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
			throw ScannerException(
			    "'SQLGetStmtAttr' for SQL_ATTR_APP_ROW_DESC failed, C type: " + std::to_string(SQL_C_NUMERIC) +
			    ", column index: " + std::to_string(col_idx) + ", column type: " + odbc_type.ToString() +
			    ",  query: '" + ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
		}
	}

	{
		SQLRETURN ret = SQLSetDescField(hdesc, col_idx, SQL_DESC_TYPE, reinterpret_cast<SQLPOINTER>(SQL_C_NUMERIC), 0);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
			throw ScannerException(
			    "'SQLSetDescField' for SQL_DESC_TYPE failed, C type: " + std::to_string(SQL_C_NUMERIC) +
			    ", column index: " + std::to_string(col_idx) + ", column type: " + odbc_type.ToString() +
			    ",  query: '" + ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
		}
	}

	{
		SQLSMALLINT precision = odbc_type.decimal_precision;
		SQLRETURN ret = SQLSetDescField(hdesc, col_idx, SQL_DESC_PRECISION, reinterpret_cast<SQLPOINTER>(precision), 0);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
			throw ScannerException(
			    "'SQLSetDescField' for SQL_DESC_PRECISION failed, C type: " + std::to_string(SQL_C_NUMERIC) +
			    ", column index: " + std::to_string(col_idx) + ", column type: " + odbc_type.ToString() +
			    ",  query: '" + ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
		}
	}

	{
		SQLSMALLINT scale = odbc_type.decimal_scale;
		SQLRETURN ret = SQLSetDescField(hdesc, col_idx, SQL_DESC_SCALE, reinterpret_cast<SQLPOINTER>(scale), 0);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
			throw ScannerException(
			    "'SQLSetDescField' for SQL_DESC_SCALE failed, C type: " + std::to_string(SQL_C_NUMERIC) +
			    ", column index: " + std::to_string(col_idx) + ", column type: " + odbc_type.ToString() +
			    ",  query: '" + ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
		}
	}
}

static void Negate(duckdb_hugeint &hi) {
	hi.lower = ~hi.lower + 1;
	hi.upper = ~hi.upper;
	if (hi.lower == 0) {
		hi.upper += 1; // carry from low to high
	}
}

static duckdb_hugeint Zero() {
	duckdb_hugeint zero;
	zero.lower = 0;
	zero.upper = 0;
	return zero;
}

// The implementation can be more robust, but we expect the decimal strings from
// drivers to be well-formed.
static duckdb_hugeint ParseHugeInt(const std::string &str) {
	duckdb_hugeint hi;
	hi.lower = 0;
	hi.upper = 0;

	for (char ch : str) {
		if (ch < '0' || ch > '9') {
			return Zero();
		}

		uint64_t digit = ch - '0';

		hi.upper *= 10;

		// check overflow
		if (hi.lower > (std::numeric_limits<uint64_t>::max() / 10)) {
			uint64_t carry = hi.lower / (std::numeric_limits<uint64_t>::max() / 10);
			hi.upper += carry;
		}

		hi.lower *= 10;
		hi.lower += digit;
	}

	return hi;
}

static void SetResult(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx, duckdb_vector vec, idx_t row_idx,
                      duckdb_hugeint hi) {
	if (odbc_type.decimal_precision <= 4) {
		int16_t *data = reinterpret_cast<int16_t *>(duckdb_vector_get_data(vec));
		data[row_idx] = static_cast<int16_t>(hi.lower);
	} else if (odbc_type.decimal_precision <= 9) {
		int32_t *data = reinterpret_cast<int32_t *>(duckdb_vector_get_data(vec));
		data[row_idx] = static_cast<int32_t>(hi.lower);
	} else if (odbc_type.decimal_precision <= 18) {
		int64_t *data = reinterpret_cast<int64_t *>(duckdb_vector_get_data(vec));
		data[row_idx] = static_cast<int64_t>(hi.lower);
	} else if (odbc_type.decimal_precision <= 38) {
		duckdb_hugeint *data = reinterpret_cast<duckdb_hugeint *>(duckdb_vector_get_data(vec));
		data[row_idx] = hi;
	} else
		throw ScannerException("Invalid unsupported DECIMAL precision: " + std::to_string(odbc_type.decimal_precision) +
		                       ", column index: " + std::to_string(col_idx) + ", column type: " + odbc_type.ToString() +
		                       ",  query: '" + ctx.query);
}

static size_t RemoveSeparatorReturnScale(std::string &str) {
	const char *dot_ptr = std::strchr(str.c_str(), '.');
	if (dot_ptr != nullptr) {
		size_t idx = static_cast<size_t>(dot_ptr - str.c_str());
		str.erase(std::remove(str.begin(), str.end(), '.'), str.end());
		str.erase(std::remove(str.begin(), str.end(), ','), str.end());
		return str.length() - idx;
	} else {
		const char *comma_ptr = std::strchr(str.c_str(), ',');
		if (comma_ptr != nullptr) {
			size_t idx = static_cast<size_t>(comma_ptr - str.c_str());
			str.erase(std::remove(str.begin(), str.end(), ','), str.end());
			return str.length() - idx;
		} else {
			return 0;
		}
	}
}

static std::pair<duckdb_hugeint, bool> FetchDecimal(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx) {
	SQLSMALLINT ctype = SQL_C_NUMERIC;
	if (ctx.quirks.decimal_columns_precision_through_ard) {
		ctype = SQL_ARD_TYPE;
	}

	SQL_NUMERIC_STRUCT fetched;
	std::memset(&fetched, '\0', sizeof(fetched));
	SQLLEN ind;
	SQLRETURN ret = SQLGetData(ctx.hstmt, col_idx, ctype, &fetched, sizeof(fetched), &ind);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' failed, C type: " + std::to_string(SQL_C_NUMERIC) + ", column index: " +
		                       std::to_string(col_idx) + ", column type: " + odbc_type.ToString() + ",  query: '" +
		                       ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}

	if (ind == SQL_NULL_DATA) {
		return std::make_pair(Zero(), true);
	}

	duckdb_hugeint hi;
	std::memcpy(&hi.lower, fetched.val, sizeof(hi.lower));
	std::memcpy(&hi.upper, fetched.val + sizeof(hi.lower), sizeof(hi.upper));

	if (fetched.sign == 0) {
		Negate(hi);
	}

	return std::make_pair(hi, false);
}

static std::pair<duckdb_hugeint, bool> FetchVarchar(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx) {
	std::vector<char> buf;
	buf.resize(128);
	SQLLEN len_bytes = 0;
	SQLRETURN ret = SQLGetData(ctx.hstmt, col_idx, SQL_C_CHAR, buf.data(), static_cast<SQLLEN>(buf.size()), &len_bytes);

	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt, SQL_HANDLE_STMT);
		throw ScannerException("'SQLGetData' for DECIMAL VARCHAR failed, column index: " + std::to_string(col_idx) +
		                       ", query: '" + ctx.query + "', column type: " + odbc_type.ToString() +
		                       ",  return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}

	if (len_bytes == SQL_NULL_DATA) {
		return std::make_pair(Zero(), true);
	}

	std::string str(buf.data(), len_bytes);

	bool negative = false;
	if ('-' == str[0]) {
		str.erase(str.begin());
		negative = true;
	}

	size_t scale = RemoveSeparatorReturnScale(str);
	(void)scale;

	if (str.empty()) {
		return std::make_pair(Zero(), false);
	}

	duckdb_hugeint parsed = ParseHugeInt(str);
	if (negative) {
		Negate(parsed);
	}

	return std::make_pair(parsed, false);
}

template <>
void TypeSpecific::SetColumnDescriptors<duckdb_decimal>(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx) {
	if (ctx.quirks.decimal_columns_precision_through_ard) {
		SetDescriptorFields(ctx, odbc_type, col_idx);
	}
}

template <>
void TypeSpecific::FetchAndSetResult<duckdb_decimal>(QueryContext &ctx, OdbcType &odbc_type, SQLSMALLINT col_idx,
                                                     duckdb_vector vec, idx_t row_idx) {
	std::pair<duckdb_hugeint, bool> fetched;
	if (ctx.quirks.decimal_columns_as_chars) {
		fetched = FetchVarchar(ctx, odbc_type, col_idx);
	} else {
		fetched = FetchDecimal(ctx, odbc_type, col_idx);
	}
	if (fetched.second) {
		Types::SetNullValueToResult(vec, row_idx);
	} else {
		SetResult(ctx, odbc_type, col_idx, vec, row_idx, fetched.first);
	}
}

template <>
duckdb_type TypeSpecific::ResolveColumnType<duckdb_decimal>(QueryContext &, ResultColumn &) {
	return DUCKDB_TYPE_DECIMAL;
}

} // namespace odbcscanner
