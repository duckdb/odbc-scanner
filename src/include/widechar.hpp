#pragma once

#include <cstdint>
#include <sql.h>
#include <sqlext.h>
#include <string>
#include <vector>

namespace odbcscanner {

struct SqlWString {
	std::vector<SQLWCHAR> vec;

	SqlWString(std::vector<SQLWCHAR> vec_in) : vec(std::move(vec_in)) {
	}

	SqlWString(SqlWString &other) = delete;
	SqlWString(SqlWString &&other) = default;

	SqlWString &operator=(const SqlWString &) = delete;
	SqlWString &operator=(SqlWString &&other) = delete;

	template <typename INT_TYPE>
	INT_TYPE length() {
		return static_cast<INT_TYPE>(vec.size() - 1);
	}

	SQLWCHAR *data() {
		return vec.data();
	}
};

std::string utf16_to_utf8_lenient(const SQLWCHAR *in_buf, size_t in_buf_len,
                                  const SQLWCHAR **first_invalid_char = nullptr);

SqlWString utf8_to_utf16_lenient(const char *in_buf, size_t in_buf_len, const char **first_invalid_char = nullptr);

size_t utf16_length(const SQLWCHAR *buf);

} // namespace odbcscanner
