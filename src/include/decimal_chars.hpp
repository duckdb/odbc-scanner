#pragma once

#include "duckdb_extension.h"

#include <vector>

namespace odbcscanner {

struct DecimalChars {
	std::vector<char> characters;

	DecimalParam();

	explicit DecimalParam(duckdb_decimal decimal);

	DecimalParam(DecimalParam &other) = delete;
	DecimalParam(DecimalParam &&other) = default;

	DecimalParam &operator=(const DecimalParam &other) = delete;
	DecimalParam &operator=(DecimalParam &&other) = default;

	template <typename INT_TYPE>
	INT_TYPE CharacterLength() {
		return static_cast<INT_TYPE>(characters.size() - 1);
	}
};

} // namespace odbcscanner
