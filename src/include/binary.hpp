#pragma once

#include <cstdint>
#include <vector>

#include "duckdb_extension_api.hpp"

namespace odbcscanner {

struct DecimalChars {
	std::vector<char> characters;

	DecimalChars();

	explicit DecimalChars(duckdb_decimal decimal);

	DecimalChars(DecimalChars &other) = delete;
	DecimalChars(DecimalChars &&other) = default;

	DecimalChars &operator=(const DecimalChars &other) = delete;
	DecimalChars &operator=(DecimalChars &&other) = default;

	template <typename INT_TYPE>
	INT_TYPE size() {
		return static_cast<INT_TYPE>(characters.size() - 1);
	}

	char *data();
};

struct OdbcBlob {
	std::vector<char> blob_data;

	OdbcBlob();

	explicit OdbcBlob(std::vector<char> blob_data_in);

	OdbcBlob(OdbcBlob &other) = delete;
	OdbcBlob(OdbcBlob &&other) = default;

	OdbcBlob &operator=(const OdbcBlob &other) = delete;
	OdbcBlob &operator=(OdbcBlob &&other) = default;

	template <typename INT_TYPE>
	INT_TYPE size() {
		return static_cast<INT_TYPE>(blob_data.size());
	}

	char *data();
};

struct OdbcUuid {
	// this can be an std::array<16> but it may be better
	// to handle it uniformly with blob
	std::vector<char> uuid_data;

	OdbcUuid();

	explicit OdbcUuid(duckdb_uhugeint uuid);

	OdbcUuid(OdbcUuid &other) = delete;
	OdbcUuid(OdbcUuid &&other) = default;

	OdbcUuid &operator=(const OdbcUuid &other) = delete;
	OdbcUuid &operator=(OdbcUuid &&other) = default;

	template <typename INT_TYPE>
	INT_TYPE size() {
		return static_cast<INT_TYPE>(uuid_data.size());
	}

	char *data();
};

struct MSSQL_GUID {
	uint32_t Data1 = 0;
	uint16_t Data2 = 0;
	uint16_t Data3 = 0;
	uint8_t Data4[8] = {0, 0, 0, 0, 0, 0, 0, 0};
};

} // namespace odbcscanner
