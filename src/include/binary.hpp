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

struct ScannerBlob {
	std::vector<char> blob_data;

	ScannerBlob();

	explicit ScannerBlob(std::vector<char> blob_data_in);

	ScannerBlob(ScannerBlob &other) = delete;
	ScannerBlob(ScannerBlob &&other) = default;

	ScannerBlob &operator=(const ScannerBlob &other) = delete;
	ScannerBlob &operator=(ScannerBlob &&other) = default;

	template <typename INT_TYPE>
	INT_TYPE size() {
		return static_cast<INT_TYPE>(blob_data.size());
	}

	char *data();
};

struct ScannerUuid {
	// this can be an std::array<16> but it may be better
	// to handle it uniformly with blob
	std::vector<char> uuid_data;

	ScannerUuid();

	explicit ScannerUuid(duckdb_uhugeint uuid);

	ScannerUuid(ScannerUuid &other) = delete;
	ScannerUuid(ScannerUuid &&other) = default;

	ScannerUuid &operator=(const ScannerUuid &other) = delete;
	ScannerUuid &operator=(ScannerUuid &&other) = default;

	template <typename INT_TYPE>
	INT_TYPE size() {
		return static_cast<INT_TYPE>(uuid_data.size());
	}

	char *data();
};

} // namespace odbcscanner
