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
	if (odbc_type.DecimalExceedsNumericStruct()) {
		// fetched as text
		return;
	}

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

namespace {

// Unsigned 128-bit integer: magnitude of the integer that backs a DECIMAL value
struct Magnitude {
	uint64_t upper = 0;
	uint64_t lower = 0;
};

} // namespace

static const uint64_t LOW_32_BITS = 0xffffffffULL;

// Multiplies the number by 10 and adds the digit to it, returns 'false' on overflow
static bool MultiplyByTenAndAdd(Magnitude &num, uint64_t digit) {
	uint64_t part0 = (num.lower & LOW_32_BITS) * 10 + digit;
	uint64_t part1 = (num.lower >> 32) * 10 + (part0 >> 32);
	uint64_t part2 = (num.upper & LOW_32_BITS) * 10 + (part1 >> 32);
	uint64_t part3 = (num.upper >> 32) * 10 + (part2 >> 32);
	if ((part3 >> 32) != 0) {
		return false;
	}
	num.lower = (part1 << 32) | (part0 & LOW_32_BITS);
	num.upper = (part3 << 32) | (part2 & LOW_32_BITS);
	return true;
}

// Divides the number by 10, returns the remainder
static uint64_t DivideByTen(Magnitude &num) {
	uint64_t parts[4] = {num.upper >> 32, num.upper & LOW_32_BITS, num.lower >> 32, num.lower & LOW_32_BITS};
	uint64_t rem = 0;
	for (uint64_t &part : parts) {
		uint64_t cur = (rem << 32) | part;
		part = cur / 10;
		rem = cur % 10;
	}
	num.upper = (parts[0] << 32) | parts[1];
	num.lower = (parts[2] << 32) | parts[3];
	return rem;
}

static bool IsZero(const Magnitude &num) {
	return num.upper == 0 && num.lower == 0;
}

// Checks that the number has at most the specified number of decimal digits, 38 digits max
static bool FitsDigits(const Magnitude &num, uint8_t digits_count) {
	// powers of ten from 10^0 to 10^38
	static const std::vector<Magnitude> powers = [] {
		std::vector<Magnitude> vec;
		Magnitude power;
		power.lower = 1;
		for (size_t i = 0; i <= 38; i++) {
			vec.push_back(power);
			MultiplyByTenAndAdd(power, 0);
		}
		return vec;
	}();
	const Magnitude &bound = powers.at(digits_count);
	return num.upper < bound.upper || (num.upper == bound.upper && num.lower < bound.lower);
}

// Formats the value the same way as DuckDB does when casting DECIMAL to VARCHAR
static std::string FormatDecimal(bool negative, std::string digits, size_t scale) {
	if (digits.length() < scale + 1) {
		digits.insert(0, scale + 1 - digits.length(), '0');
	}
	std::string res;
	if (negative) {
		res.push_back('-');
	}
	res.append(digits, 0, digits.length() - scale);
	if (scale > 0) {
		res.push_back('.');
		res.append(digits, digits.length() - scale, scale);
	}
	return res;
}

static std::string FormatDecimal(bool negative, Magnitude num, size_t scale) {
	std::string digits;
	while (!IsZero(num)) {
		digits.push_back(static_cast<char>('0' + DivideByTen(num)));
	}
	std::reverse(digits.begin(), digits.end());
	return FormatDecimal(negative, std::move(digits), scale);
}

// Source values are never rounded, truncated or wrapped around to make them fit the column type
static void ThrowDoesNotFit(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx,
                            const std::string &reason, const std::string &value_text) {
	static const size_t max_text_len = 128;
	std::string text = value_text.length() <= max_text_len ? value_text : value_text.substr(0, max_text_len) + "...";
	throw ScannerException("DECIMAL value fetched from the data source cannot be represented as DECIMAL(" +
	                       std::to_string(odbc_type.decimal_precision) + "," + std::to_string(odbc_type.decimal_scale) +
	                       ") without changing it: " + reason + ", value: '" + text +
	                       "', column index: " + std::to_string(col_idx) + ", column type: " + odbc_type.ToString() +
	                       ", query: '" + ctx.query +
	                       "'. Cast this column in the remote query to a DECIMAL type that DuckDB can hold, " +
	                       "or to a character type to fetch it as VARCHAR.");
}

static void SetResult(const OdbcType &odbc_type, Vector &vec, idx_t row_idx, const Magnitude &num, bool negative) {
	int128_t hi;
	hi.lower = num.lower;
	hi.upper = static_cast<int64_t>(num.upper);
	if (negative) {
		hi.lower = ~hi.lower + 1;
		hi.upper = ~hi.upper;
		if (hi.lower == 0) {
			hi.upper += 1; // carry from low to high
		}
	}

	// The value is checked to have no more digits than the precision,
	// so it fits the storage tier that is defined by the precision.
	if (odbc_type.decimal_precision <= 4) {
		int16_t *data = vec.GetDataMutable<int16_t>();
		data[row_idx] = static_cast<int16_t>(hi.lower);
	} else if (odbc_type.decimal_precision <= 9) {
		int32_t *data = vec.GetDataMutable<int32_t>();
		data[row_idx] = static_cast<int32_t>(hi.lower);
	} else if (odbc_type.decimal_precision <= 18) {
		int64_t *data = vec.GetDataMutable<int64_t>();
		data[row_idx] = static_cast<int64_t>(hi.lower);
	} else {
		int128_t *data = vec.GetDataMutable<int128_t>();
		data[row_idx] = hi;
	}
}

static bool IsTruncationDiagCode(const std::string &code) {
	// string data right truncated, fractional truncation, numeric value out of range
	return code == "01004" || code == "01S07" || code == "22003";
}

static void FetchNumericStructAndSetResult(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx,
                                           Vector &vec, idx_t row_idx) {
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
		// SQL_NUMERIC_STRUCT holds 16 bytes of the unscaled value, the driver reports a warning
		// when the source value does not fit there, the fetched struct is not usable in this case.
		if (ret == SQL_SUCCESS_WITH_INFO && ind != SQL_NULL_DATA &&
		    IsTruncationDiagCode(Diagnostics::ReadCode(ctx.hstmt(), SQL_HANDLE_STMT))) {
			std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
			throw ScannerException("DECIMAL value fetched from the data source was truncated by the ODBC driver, it "
			                       "does not fit into SQL_NUMERIC_STRUCT, column index: " +
			                       std::to_string(col_idx) + ", column type: " + odbc_type.ToString() + ", query: '" +
			                       ctx.query + "', diagnostics: '" + diag +
			                       "'. Use 'decimal_columns_as_chars=TRUE' option to fetch DECIMAL values as text, " +
			                       "or cast this column in the remote query to a narrower DECIMAL type.");
		}
		fetched_ptr = &fetched_data;
	}

	SQL_NUMERIC_STRUCT &fetched = *fetched_ptr;

	if (ind == SQL_NULL_DATA) {
		Types::SetNullValueToResult(vec, row_idx);
		return;
	}

	// unscaled value is stored in little-endian
	Magnitude num;
	for (size_t i = 0; i < 8; i++) {
		num.lower |= static_cast<uint64_t>(fetched.val[i]) << (i * 8);
		num.upper |= static_cast<uint64_t>(fetched.val[i + 8]) << (i * 8);
	}
	bool negative = fetched.sign == 0 && !IsZero(num);

	// The driver can return the value with a scale that is different from the scale of the column (for example,
	// with the scale of this particular value), unless the scale was requested explicitly through the ARD.
	int64_t value_scale = static_cast<int64_t>(odbc_type.decimal_scale);
	if (!ctx.quirks.decimal_columns_precision_through_ard) {
		value_scale = static_cast<int64_t>(fetched.scale);
	}
	// text of the value as it was fetched, only used in error messages
	const Magnitude num_fetched = num;
	const int64_t scale_fetched = value_scale;
	auto fetched_text = [&num_fetched, scale_fetched, negative]() {
		if (scale_fetched >= 0) {
			return FormatDecimal(negative, num_fetched, static_cast<size_t>(scale_fetched));
		}
		return FormatDecimal(negative, num_fetched, 0) + std::string(static_cast<size_t>(-scale_fetched), '0');
	};
	int64_t column_scale = static_cast<int64_t>(odbc_type.decimal_scale);
	for (; value_scale < column_scale; value_scale++) {
		if (!MultiplyByTenAndAdd(num, 0)) {
			ThrowDoesNotFit(ctx, odbc_type, col_idx, "value has more digits than the column precision", fetched_text());
		}
	}
	for (; value_scale > column_scale; value_scale--) {
		if (DivideByTen(num) != 0) {
			ThrowDoesNotFit(ctx, odbc_type, col_idx, "value has more fractional digits than the column scale",
			                fetched_text());
		}
	}

	if (!FitsDigits(num, odbc_type.decimal_precision)) {
		ThrowDoesNotFit(ctx, odbc_type, col_idx, "value has more digits than the column precision", fetched_text());
	}

	SetResult(odbc_type, vec, row_idx, num, negative);
}

// Reads the value as text, in multiple parts when it does not fit into the buffer, returns 'false' for NULL
static bool FetchText(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx, std::string &text) {
	std::vector<char> buf;
	buf.resize(512);
	const size_t max_part_len = buf.size() - 1;

	for (size_t part_num = 0;; part_num++) {
		SQLLEN len_bytes = 0;
		SQLRETURN ret =
		    SQLGetData(ctx.hstmt(), col_idx, SQL_C_CHAR, buf.data(), static_cast<SQLLEN>(buf.size()), &len_bytes);

		if (ret == SQL_NO_DATA && part_num > 0) {
			// previous part was the last one
			return true;
		}

		if (!SQL_SUCCEEDED(ret)) {
			std::string diag = Diagnostics::Read(ctx.hstmt(), SQL_HANDLE_STMT);
			throw ScannerException("'SQLGetData' for DECIMAL VARCHAR failed, column index: " + std::to_string(col_idx) +
			                       ", query: '" + ctx.query + "', column type: " + odbc_type.ToString() +
			                       ",  return: " + std::to_string(ret) + ", diagnostics: '" + diag + "'");
		}

		if (len_bytes == SQL_NULL_DATA) {
			return false;
		}

		bool has_more_parts = ret == SQL_SUCCESS_WITH_INFO &&
		                      (len_bytes == SQL_NO_TOTAL || len_bytes > static_cast<SQLLEN>(max_part_len));

		if (!has_more_parts) {
			if (len_bytes < 0 || len_bytes > static_cast<SQLLEN>(max_part_len)) {
				throw ScannerException("'SQLGetData' for DECIMAL VARCHAR returned invalid length: " +
				                       std::to_string(len_bytes) + ", column index: " + std::to_string(col_idx) +
				                       ", query: '" + ctx.query + "', column type: " + odbc_type.ToString());
			}
			text.append(buf.data(), static_cast<size_t>(len_bytes));
			return true;
		}

		if (ctx.quirks.var_len_data_single_part) {
			throw ScannerException("DECIMAL value fetched as text is longer than " + std::to_string(max_part_len) +
			                       " characters and the driver does not support reading it in parts, column index: " +
			                       std::to_string(col_idx) + ", query: '" + ctx.query +
			                       "', column type: " + odbc_type.ToString());
		}

		text.append(buf.data(), max_part_len);
	}
}

// Parses the exponent that follows the mantissa of the value, 'pos' points to the character after the 'E'
static bool ParseDecimalExponent(const std::string &text, size_t pos, size_t end, int64_t &exponent) {
	// larger exponents cannot belong to a value that fits any DECIMAL
	static const int64_t max_exponent = 100000;
	bool negative = false;
	if (pos < end && (text[pos] == '-' || text[pos] == '+')) {
		negative = text[pos] == '-';
		pos++;
	}
	if (pos == end) {
		return false;
	}
	exponent = 0;
	for (; pos < end; pos++) {
		char ch = text[pos];
		if (ch < '0' || ch > '9') {
			return false;
		}
		exponent = exponent * 10 + static_cast<int64_t>(ch - '0');
		if (exponent > max_exponent) {
			return false;
		}
	}
	if (negative) {
		exponent = -exponent;
	}
	return true;
}

// Extracts the sign, the digits and the number of fractional digits from the text representation of the value.
// Decimal separator can be either dot or comma. When both are present, commas are treated as group separators.
// Scientific notation (like '1.5E-7') is accepted too.
static bool ParseDecimalText(const std::string &text, bool &negative, std::string &digits, size_t &scale) {
	size_t begin = 0;
	size_t end = text.length();
	while (begin < end && text[begin] == ' ') {
		begin++;
	}
	while (end > begin && text[end - 1] == ' ') {
		end--;
	}

	negative = false;
	if (begin < end && (text[begin] == '-' || text[begin] == '+')) {
		negative = text[begin] == '-';
		begin++;
	}

	bool has_dot = text.find('.', begin) != std::string::npos;
	bool separator_seen = false;
	digits.clear();
	scale = 0;
	for (size_t i = begin; i < end; i++) {
		char ch = text[i];
		if (ch >= '0' && ch <= '9') {
			digits.push_back(ch);
			if (separator_seen) {
				scale++;
			}
		} else if (ch == '.' || (ch == ',' && !has_dot)) {
			if (separator_seen) {
				return false;
			}
			separator_seen = true;
		} else if (ch == ',' && !separator_seen) {
			// group separator
		} else if ((ch == 'e' || ch == 'E') && !digits.empty()) {
			int64_t exponent = 0;
			if (!ParseDecimalExponent(text, i + 1, end, exponent)) {
				return false;
			}
			if (exponent < 0) {
				scale += static_cast<size_t>(-exponent);
			} else if (static_cast<size_t>(exponent) <= scale) {
				scale -= static_cast<size_t>(exponent);
			} else {
				digits.append(static_cast<size_t>(exponent) - scale, '0');
				scale = 0;
			}
			break;
		} else {
			return false;
		}
	}
	if (digits.empty()) {
		return false;
	}
	// the number can have no integer digits
	if (digits.length() < scale) {
		digits.insert(0, scale - digits.length(), '0');
	}
	return true;
}

static void FetchTextAndSetResult(const QueryContext &ctx, const OdbcType &odbc_type, SQLSMALLINT col_idx, Vector &vec,
                                  idx_t row_idx) {
	std::string text;
	if (!FetchText(ctx, odbc_type, col_idx, text)) {
		Types::SetNullValueToResult(vec, row_idx);
		return;
	}

	bool negative = false;
	std::string digits;
	size_t scale = 0;
	if (!ParseDecimalText(text, negative, digits, scale)) {
		ThrowDoesNotFit(ctx, odbc_type, col_idx, "value is not a finite decimal number", text);
	}

	// bring the digits to the scale of the column
	size_t column_scale = static_cast<size_t>(odbc_type.decimal_scale);
	if (scale > column_scale) {
		size_t extra = scale - column_scale;
		if (digits.find_first_not_of('0', digits.length() - extra) != std::string::npos) {
			ThrowDoesNotFit(ctx, odbc_type, col_idx, "value has more fractional digits than the column scale", text);
		}
		digits.erase(digits.length() - extra);
	} else {
		digits.append(column_scale - scale, '0');
	}

	size_t leading_zeros = digits.find_first_not_of('0');
	digits.erase(0, leading_zeros != std::string::npos ? leading_zeros : digits.length());

	if (digits.length() > static_cast<size_t>(odbc_type.decimal_precision)) {
		ThrowDoesNotFit(ctx, odbc_type, col_idx, "value has more digits than the column precision", text);
	}

	// cannot overflow, the number has 38 digits at most
	Magnitude num;
	for (char ch : digits) {
		MultiplyByTenAndAdd(num, static_cast<uint64_t>(ch - '0'));
	}

	SetResult(odbc_type, vec, row_idx, num, negative && !IsZero(num));
}

template <>
void TypeSpecific::FetchAndSetResult<DecimalValue>(const QueryContext &ctx, const OdbcType &odbc_type,
                                                   SQLSMALLINT col_idx, Vector &vec, idx_t row_idx) {
	if (ctx.quirks.decimal_columns_as_chars || odbc_type.DecimalExceedsNumericStruct()) {
		FetchTextAndSetResult(ctx, odbc_type, col_idx, vec, row_idx);
	} else {
		FetchNumericStructAndSetResult(ctx, odbc_type, col_idx, vec, row_idx);
	}
}

template <>
LogicalTypeId TypeSpecific::ResolveColumnType<DecimalValue>(const QueryContext &, ResultColumn &) {
	return LogicalTypeId::DECIMAL;
}

} // namespace odbcscanner
