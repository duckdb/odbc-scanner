#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <sql.h>
#include <sqlext.h>

namespace odbcscanner {

struct WideString {
	std::vector<SQLWCHAR> vec;

	WideString() {
	}

	explicit WideString(std::vector<SQLWCHAR> vec_in) : vec(std::move(vec_in)) {
	}

	WideString(WideString &other) = delete;
	WideString(WideString &&other) = default;

	WideString &operator=(const WideString &other) = delete;
	WideString &operator=(WideString &&other) = default;

	template <typename INT_TYPE>
	INT_TYPE length() {
		return static_cast<INT_TYPE>(vec.size() - 1);
	}

	SQLWCHAR *data() {
		return vec.data();
	}
};

struct WideChar {
	static std::string Narrow(const SQLWCHAR *in_buf, size_t in_buf_len, const SQLWCHAR **first_invalid_char = nullptr);

	static WideString Widen(const char *in_buf, size_t in_buf_len, const char **first_invalid_char = nullptr);
};

} // namespace odbcscanner
