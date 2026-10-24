#include "binary.hpp"

#include <cstring>
#include <algorithm>
#include <string>

#include "function_args.hpp"
#include "duckdb_extension_api.hpp"
#include "odbc_api.hpp"
#include "scanner_exception.hpp"

namespace odbcscanner {

DecimalChars::DecimalChars() {
	this->characters.push_back('\0');
}

// Formats DECIMAL value the same way as DuckDB does when casting it to VARCHAR
static std::string DecimalToString(DecimalValue decimal) {
	uint64_t upper = static_cast<uint64_t>(decimal.value.upper);
	uint64_t lower = decimal.value.lower;
	bool negative = decimal.value.upper < 0;
	if (negative) {
		// negate
		lower = ~lower + 1;
		upper = ~upper;
		if (lower == 0) {
			upper += 1; // carry from low to high
		}
	}

	std::string digits;
	while (upper != 0 || lower != 0) {
		// divide 128-bit unsigned number by 10 using 32-bit parts of the lower half
		uint64_t rem = upper % 10;
		upper /= 10;
		uint64_t lower_hi = (rem << 32) | (lower >> 32);
		uint64_t q1 = lower_hi / 10;
		uint64_t r1 = lower_hi % 10;
		uint64_t lower_lo = (r1 << 32) | (lower & 0xffffffff);
		uint64_t q2 = lower_lo / 10;
		uint64_t r2 = lower_lo % 10;
		lower = (q1 << 32) | q2;
		digits.push_back(static_cast<char>('0' + r2));
	}
	while (digits.length() < static_cast<size_t>(decimal.scale) + 1) {
		digits.push_back('0');
	}
	std::reverse(digits.begin(), digits.end());

	std::string res;
	if (negative) {
		res.push_back('-');
	}
	if (decimal.scale > 0) {
		size_t int_len = digits.length() - decimal.scale;
		res.append(digits, 0, int_len);
		res.push_back('.');
		res.append(digits, int_len, std::string::npos);
	} else {
		res.append(digits);
	}
	return res;
}

DecimalChars::DecimalChars(DecimalValue decimal) {
	std::string chars = DecimalToString(decimal);
	size_t len = chars.length();
	this->characters.resize(len + 1);
	std::memcpy(this->characters.data(), chars.c_str(), len);
}

char *DecimalChars::data() {
	return characters.data();
}

SQLWCHAR *DecimalChars::wide_data() {
	return wide_characters.data();
}

ScannerBlob::ScannerBlob() {
}

ScannerBlob::ScannerBlob(std::vector<char> blob_data_in) : blob_data(std::move(blob_data_in)) {
}

char *ScannerBlob::data() {
	return blob_data.data();
}

ScannerUuid::ScannerUuid() {
	uuid_data.resize(16);
}

ScannerUuid::ScannerUuid(uint128_t uuid) : ScannerUuid() {
	SQLGUID guid;
	guid.Data1 = static_cast<uint32_t>(uuid.upper >> 32);
	guid.Data2 = static_cast<uint16_t>((uuid.upper >> 16) & 0xffff);
	guid.Data3 = static_cast<uint16_t>(uuid.upper & 0xffff);
	guid.Data4[0] = static_cast<uint8_t>((uuid.lower >> 56) & 0xff);
	guid.Data4[1] = static_cast<uint8_t>((uuid.lower >> 48) & 0xff);
	guid.Data4[2] = static_cast<uint8_t>((uuid.lower >> 40) & 0xff);
	guid.Data4[3] = static_cast<uint8_t>((uuid.lower >> 32) & 0xff);
	guid.Data4[4] = static_cast<uint8_t>((uuid.lower >> 24) & 0xff);
	guid.Data4[5] = static_cast<uint8_t>((uuid.lower >> 16) & 0xff);
	guid.Data4[6] = static_cast<uint8_t>((uuid.lower >> 8) & 0xff);
	guid.Data4[7] = static_cast<uint8_t>((uuid.lower) & 0xff);
	std::memcpy(uuid_data.data(), &guid, sizeof(guid));
}

char *ScannerUuid::data() {
	return uuid_data.data();
}

} // namespace odbcscanner