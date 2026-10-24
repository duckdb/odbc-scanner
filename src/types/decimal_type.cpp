#include "types.hpp"

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <limits>

#include "function_args.hpp"
#include "connection.hpp"
#include "diagnostics.hpp"
#include "scanner_exception.hpp"
#include "widechar.hpp"

namespace odbcscanner {

// Storage tier of DECIMAL values is defined by the width
static LogicalTypeId DecimalInternalType(uint8_t width) {
	if (width <= 4) {
		return LogicalTypeId::SMALLINT;
	} else if (width <= 9) {
		return LogicalTypeId::INTEGER;
	} else if (width <= 18) {
		return LogicalTypeId::BIGINT;
	} else if (width <= 38) {
		return LogicalTypeId::HUGEINT;
	}
	return LogicalTypeId::INVALID;
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<DecimalValue>(const DbmsQuirks &quirks, Vector &vec, idx_t row_idx) {
	auto ltype = vec.GetLogicalType();

	DecimalValue dec;
	dec.width = ltype.GetDecimalWidth();
	dec.scale = ltype.GetDecimalScale();
	dec.value.lower = 0;
	dec.value.upper = 0;

	LogicalTypeId type_id = DecimalInternalType(dec.width);
	switch (type_id) {
	case LogicalTypeId::SMALLINT: {
		int16_t *data = vec.GetDataMutable<int16_t>();
		int16_t val = data[row_idx];
		dec.value.lower = static_cast<uint64_t>(val);
		dec.value.upper = val >= 0 ? 0 : -1;
		break;
	}
	case LogicalTypeId::INTEGER: {
		int32_t *data = vec.GetDataMutable<int32_t>();
		int32_t val = data[row_idx];
		dec.value.lower = static_cast<uint64_t>(val);
		dec.value.upper = val >= 0 ? 0 : -1;
		break;
	}
	case LogicalTypeId::BIGINT: {
		int64_t *data = vec.GetDataMutable<int64_t>();
		int64_t val = data[row_idx];
		dec.value.lower = static_cast<uint64_t>(val);
		dec.value.upper = val >= 0 ? 0 : -1;
		break;
	}
	case LogicalTypeId::HUGEINT: {
		int128_t *data = vec.GetDataMutable<int128_t>();
		dec.value = data[row_idx];
		break;
	}
	default:
		throw ScannerException("Invalid unsupported DECIMAL internal type, ID: " +
		                       std::to_string(static_cast<uint32_t>(type_id)));
	}

	return ScannerValue(dec, quirks.decimal_params_as_chars);
}

// Parses DECIMAL value text, as rendered by DuckDB, into its backing integer
static int128_t DecimalStorageFromText(const std::string &text) {
	uint64_t upper = 0;
	uint64_t lower = 0;
	bool negative = false;
	for (char ch : text) {
		if (ch == '-') {
			negative = true;
			continue;
		}
		if (ch < '0' || ch > '9') {
			continue;
		}
		// multiply 128-bit unsigned number by 10 and add digit using 32-bit parts of the lower half
		uint64_t lower_lo = (lower & 0xffffffff) * 10 + static_cast<uint64_t>(ch - '0');
		uint64_t lower_hi = (lower >> 32) * 10 + (lower_lo >> 32);
		lower = (lower_hi << 32) | (lower_lo & 0xffffffff);
		upper = upper * 10 + (lower_hi >> 32);
	}
	if (negative) {
		lower = ~lower + 1;
		upper = ~upper;
		if (lower == 0) {
			upper += 1; // carry from low to high
		}
	}
	int128_t res;
	res.lower = lower;
	res.upper = static_cast<int64_t>(upper);
	return res;
}

template <>
ScannerValue TypeSpecific::ExtractNotNullParam<DecimalValue>(const DbmsQuirks &quirks, const Value *value) {
	// C++ API reads DECIMAL backing integers only for the width and scale known at compile time
	LogicalType ltype = value->GetLogicalType();
	DecimalValue val;
	val.width = ltype.GetDecimalWidth();
	val.scale = ltype.GetDecimalScale();
	val.value = DecimalStorageFromText(value->ToText());
	return ScannerValue(val, quirks.decimal_params_as_chars);
}

template <>
void TypeSpecific::BindOdbcParam<SQL_NUMERIC_STRUCT>(const QueryContext &ctx, ScannerValue &param,
                                                     SQLSMALLINT param_idx) {
	SQLSMALLINT sqltype = param.ExpectedType() != SQL_PARAM_TYPE_UNKNOWN ? param.ExpectedType() : SQL_NUMERIC;
	SQL_NUMERIC_STRUCT &ns = param.Value<SQL_NUMERIC_STRUCT>();
	SQLRETURN ret = SQLBindParameter(ctx.hstmt(), param_idx, SQL_PARAM_INPUT, SQL_C_NUMERIC, sqltype,
	                                 static_cast<SQLULEN>(ns.precision), static_cast<SQLSMALLINT>(ns.scale),
	                                 reinterpret_cast<SQLPOINTER>(&ns), param.LengthBytes(), &param.LengthBytes());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' failed, type: " + std::to_string(sqltype) +
		                       ", index: " + std::to_string(param_idx) + ", query: '" + ctx.query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

template <>
void TypeSpecific::BindOdbcParam<DecimalChars>(const QueryContext &ctx, ScannerValue &param, SQLSMALLINT param_idx) {
	SQLSMALLINT expected = param.ExpectedType();
	// Preserve the driver's column type when it is a CHAR/VARCHAR/WCHAR family:
	// binding → the actual expected type avoids an extra driver-side coercion.
	// For non-character expected types (e.g. SQL_NUMERIC), fall back to SQL_VARCHAR
	// and let the driver parse the char buffer into the target type.
	SQLSMALLINT sqltype = Types::IsCharacterSQLType(expected) ? expected : SQL_VARCHAR;
	DecimalChars &dc = param.Value<DecimalChars>();

	// Wide targets: widen the ASCII digits to SQLWCHAR and bind SQL_C_WCHAR.
	// Some drivers do not reliably auto-convert SQL_C_CHAR → SQL_W*CHAR.
	if (Types::IsWideCharacterSQLType(expected)) {
		if (dc.wide_characters.empty()) {
			WideString wstr = WideChar::Widen(dc.data(), dc.size<size_t>());
			dc.wide_characters = std::move(wstr.vec);
		}
		SQLLEN length_chars = static_cast<SQLLEN>(dc.wide_characters.size() - 1);
		param.SetLengthBytes(length_chars * static_cast<SQLLEN>(sizeof(SQLWCHAR)));
		SQLRETURN ret = SQLBindParameter(
		    ctx.hstmt(), param_idx, SQL_PARAM_INPUT, SQL_C_WCHAR, sqltype, static_cast<SQLULEN>(length_chars), 0,
		    reinterpret_cast<SQLPOINTER>(dc.wide_data()), param.LengthBytes(), &param.LengthBytes());
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
			throw ScannerException("'SQLBindParameter' failed, type: " + std::to_string(sqltype) +
			                       ", index: " + std::to_string(param_idx) + ", query: '" + ctx.query +
			                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
		}
		return;
	}

	SQLRETURN ret =
	    SQLBindParameter(ctx.hstmt(), param_idx, SQL_PARAM_INPUT, SQL_C_CHAR, sqltype, param.LengthBytes(), 0,
	                     reinterpret_cast<SQLPOINTER>(dc.data()), param.LengthBytes(), &param.LengthBytes());
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindParameter' failed, type: " + std::to_string(sqltype) +
		                       ", index: " + std::to_string(param_idx) + ", query: '" + ctx.query +
		                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

static void SetDescriptorFields(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx) {
	SQLHDESC hdesc = nullptr;
	{
		SQLRETURN ret = SQLGetStmtAttr(ctx.hstmt(), SQL_ATTR_APP_ROW_DESC, &hdesc, 0, NULL);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
			throw ScannerException(
			    "'SQLGetStmtAttr' for SQL_ATTR_APP_ROW_DESC failed, C type: " + std::to_string(SQL_C_NUMERIC) +
			    ", column index: " + std::to_string(col_idx) + ", column type: " + odbc_type.ToString() +
			    ",  query: '" + ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
		}
	}

	{
		SQLRETURN ret = SQLSetDescField(hdesc, col_idx, SQL_DESC_TYPE, reinterpret_cast<SQLPOINTER>(SQL_C_NUMERIC), 0);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
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
			std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
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
			std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
			throw ScannerException(
			    "'SQLSetDescField' for SQL_DESC_SCALE failed, C type: " + std::to_string(SQL_C_NUMERIC) +
			    ", column index: " + std::to_string(col_idx) + ", column type: " + odbc_type.ToString() +
			    ",  query: '" + ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
		}
	}
}

static void ZeroNumericStruct(SQL_NUMERIC_STRUCT &ns) {
	ns.precision = 0;
	ns.scale = 0;
	ns.sign = 0;
	for (size_t i = 0; i < SQL_MAX_NUMERIC_LEN; i++) {
		ns.val[i] = 0;
	}
}

template <>
void TypeSpecific::BindColumn<DecimalValue>(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx) {
	if (ctx.quirks.decimal_columns_precision_through_ard) {
		SetDescriptorFields(ctx, odbc_type, col_idx);
	}

	if (!ctx.quirks.enable_columns_binding || ctx.quirks.decimal_columns_as_chars) {
		return;
	}

	SQLSMALLINT ctype = SQL_C_NUMERIC;
	if (ctx.quirks.decimal_columns_as_ard_type) {
		ctype = SQL_ARD_TYPE;
	}

	SQL_NUMERIC_STRUCT nstruct;
	ZeroNumericStruct(nstruct);
	ScannerValue nval(nstruct);
	ColumnBind nbind(std::move(nval));

	ColumnBind &bind = ctx.BindForColumn(col_idx);
	bind = std::move(nbind);
	SQL_NUMERIC_STRUCT &fetched = bind.Value<SQL_NUMERIC_STRUCT>();
	SQLLEN &ind = bind.Indicator();
	SQLRETURN ret = SQLBindCol(ctx.hstmt(), col_idx, ctype, &fetched, sizeof(SQL_NUMERIC_STRUCT), &ind);
	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
		throw ScannerException("'SQLBindCol' failed, C type: " + std::to_string(SQL_C_NUMERIC) + ", column index: " +
		                       std::to_string(col_idx) + ", column type: " + odbc_type.ToString() + ",  query: '" +
		                       ctx.query + "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
	}
}

static void Negate(int128_t &hi) {
	hi.lower = ~hi.lower + 1;
	hi.upper = ~hi.upper;
	if (hi.lower == 0) {
		hi.upper += 1; // carry from low to high
	}
}

static int128_t Zero() {
	int128_t zero;
	zero.lower = 0;
	zero.upper = 0;
	return zero;
}

// The implementation can be more robust, but we expect the decimal strings from
// drivers to be well-formed.
static int128_t ParseHugeInt(const std::string &str) {
	int128_t hi;
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

static void SetResult(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx, Vector &vec,
                      idx_t row_idx, int128_t hi) {
	if (odbc_type.decimal_precision <= 4) {
		int16_t *data = vec.GetDataMutable<int16_t>();
		data[row_idx] = static_cast<int16_t>(hi.lower);
	} else if (odbc_type.decimal_precision <= 9) {
		int32_t *data = vec.GetDataMutable<int32_t>();
		data[row_idx] = static_cast<int32_t>(hi.lower);
	} else if (odbc_type.decimal_precision <= 18) {
		int64_t *data = vec.GetDataMutable<int64_t>();
		data[row_idx] = static_cast<int64_t>(hi.lower);
	} else if (odbc_type.decimal_precision <= 38) {
		int128_t *data = vec.GetDataMutable<int128_t>();
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

static std::pair<int128_t, bool> FetchDecimal(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx) {
	SQL_NUMERIC_STRUCT fetched_data;
	ZeroNumericStruct(fetched_data);
	SQL_NUMERIC_STRUCT *fetched_ptr = &fetched_data;
	SQLLEN ind = 0;

	if (ctx.quirks.enable_columns_binding) {
		ColumnBind &bind = ctx.BindForColumn(col_idx);
		SQL_NUMERIC_STRUCT &bound = bind.Value<SQL_NUMERIC_STRUCT>();
		fetched_ptr = &bound;
		ind = bind.ind;
	} else {
		SQLSMALLINT ctype = SQL_C_NUMERIC;
		if (ctx.quirks.decimal_columns_as_ard_type) {
			ctype = SQL_ARD_TYPE;
		}
		SQLRETURN ret = SQLGetData(ctx.hstmt(), col_idx, ctype, &fetched_data, sizeof(fetched_data), &ind);
		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
			throw ScannerException("'SQLGetData' failed, C type: " + std::to_string(SQL_C_NUMERIC) +
			                       ", column index: " + std::to_string(col_idx) +
			                       ", column type: " + odbc_type.ToString() + ",  query: '" + ctx.query +
			                       "', return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
		}
		fetched_ptr = &fetched_data;
	}

	SQL_NUMERIC_STRUCT &fetched = *fetched_ptr;

	if (ind == SQL_NULL_DATA) {
		return std::make_pair(Zero(), true);
	}

	int128_t hi;
	std::memcpy(&hi.lower, fetched.val, sizeof(hi.lower));
	std::memcpy(&hi.upper, fetched.val + sizeof(hi.lower), sizeof(hi.upper));

	if (fetched.sign == 0) {
		Negate(hi);
	}

	return std::make_pair(hi, false);
}

static std::pair<int128_t, bool> FetchVarchar(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx) {
	std::vector<char> buf;
	buf.resize(128);
	SQLLEN len_bytes = 0;
	SQLRETURN ret =
	    SQLGetData(ctx.hstmt(), col_idx, SQL_C_CHAR, buf.data(), static_cast<SQLLEN>(buf.size()), &len_bytes);

	if (!SQL_SUCCEEDED(ret)) {
		std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
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

	int128_t parsed = ParseHugeInt(str);
	if (negative) {
		Negate(parsed);
	}

	return std::make_pair(parsed, false);
}

template <>
void TypeSpecific::FetchAndSetResult<DecimalValue>(const QueryContext &ctx, const OdbcType &odbc_type,
                                                   SQLSMALLINT col_idx, Vector &vec, idx_t row_idx) {
	std::pair<int128_t, bool> fetched;
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
LogicalTypeId TypeSpecific::ResolveColumnType<DecimalValue>(const QueryContext &, ResultColumn &) {
	return LogicalTypeId::DECIMAL;
}

} // namespace odbcscanner
