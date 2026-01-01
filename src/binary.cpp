#include "binary.hpp"

#include <cstring>
#include <algorithm>

#include "capi_pointers.hpp"
#include "duckdb_extension_api.hpp"
#include "odbc_api.hpp"
#include "scanner_exception.hpp"

namespace odbcscanner {

DecimalChars::DecimalChars() {
	this->characters.push_back('\0');
}

DecimalChars::DecimalChars(duckdb_decimal decimal) {
	duckdb_value val = duckdb_create_decimal(decimal);
	auto chars = VarcharPtr(duckdb_get_varchar(val), VarcharDeleter);
	if (chars.get() == nullptr) {
		throw ScannerException("DECIMAL parameter conversion error");
	}
	size_t len = std::strlen(chars.get());
	this->characters.resize(len + 1);
	std::memcpy(this->characters.data(), chars.get(), len);
}

char *DecimalChars::data() {
	return characters.data();
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

ScannerUuid::ScannerUuid(duckdb_uhugeint uuid) : ScannerUuid() {
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