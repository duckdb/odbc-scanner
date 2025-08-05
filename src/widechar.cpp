#include "widechar.hpp"

#include <limits>

#define UTF_CPP_CPLUSPLUS 199711L
#include "utf8.h"

namespace odbcscanner {

static const uint32_t invalid_char_replacement = 0xfffd;

template <typename u16bit_iterator>
static u16bit_iterator utf16_find_invalid(u16bit_iterator start, u16bit_iterator end) {
	while (start != end) {
		utf8::utfchar32_t cp = utf8::internal::mask16(*start++);
		if (utf8::internal::is_lead_surrogate(cp)) { // Take care of surrogate pairs first
			if (start != end) {
				const utf8::utfchar32_t trail_surrogate = utf8::internal::mask16(*start++);
				if (!utf8::internal::is_trail_surrogate(trail_surrogate)) {
					return start - 1;
				}
			} else {
				return start - 1;
			}
		} else if (utf8::internal::is_trail_surrogate(cp)) { // Lone trail surrogate
			return start;
		}
	}
	return end;
}

template <typename u16bit_iterator, typename output_iterator>
static output_iterator utf16_replace_invalid(u16bit_iterator start, u16bit_iterator end, output_iterator out,
                                             utf8::utfchar32_t replacement) {
	while (start != end) {
		uint16_t char1 = *start++;
		utf8::utfchar32_t cp = utf8::internal::mask16(char1);
		if (utf8::internal::is_lead_surrogate(cp)) { // Take care of surrogate pairs first
			if (start != end) {
				uint16_t char2 = *start++;
				const utf8::utfchar32_t trail_surrogate = utf8::internal::mask16(char2);
				if (utf8::internal::is_trail_surrogate(trail_surrogate)) {
					*out++ = char1;
					*out++ = char2;
				} else {
					*out++ = replacement;
					*out++ = char2;
				}
			} else {
				*out++ = replacement;
			}
		} else if (utf8::internal::is_trail_surrogate(cp)) { // Lone trail surrogate
			*out++ = replacement;
		} else {
			*out++ = char1;
		}
	}
	return out;
}

std::string utf16_to_utf8_lenient(const SQLWCHAR *in_buf, size_t in_buf_len, const SQLWCHAR **first_invalid_char) {
	std::string res;
	auto res_bi = std::back_inserter(res);

	const SQLWCHAR *in_buf_end = in_buf + in_buf_len;
	const SQLWCHAR *first_invalid_found = utf16_find_invalid(in_buf, in_buf_end);

	if (first_invalid_found == in_buf_end) {
		utf8::unchecked::utf16to8(in_buf, in_buf_end, res_bi);
		if (first_invalid_char != nullptr) {
			*first_invalid_char = nullptr;
		}
	} else {
		std::vector<SQLWCHAR> replaced;
		auto replaced_bi = std::back_inserter(replaced);
		utf16_replace_invalid(in_buf, in_buf_end, replaced_bi, invalid_char_replacement);
		utf8::unchecked::utf16to8(replaced.begin(), replaced.end(), res_bi);
		if (first_invalid_char != nullptr) {
			*first_invalid_char = first_invalid_found;
		}
	}

	return res;
}

SqlWString utf8_to_utf16_lenient(const char *in_buf, size_t in_buf_len, const char **first_invalid_char) {
	std::vector<SQLWCHAR> res;
	auto res_bi = std::back_inserter(res);

	const char *in_buf_end = in_buf + in_buf_len;
	const char *first_invalid_found = utf8::find_invalid(in_buf, in_buf_end);

	if (first_invalid_found == in_buf_end) {
		utf8::unchecked::utf8to16(in_buf, in_buf_end, res_bi);
		if (first_invalid_char != nullptr) {
			*first_invalid_char = nullptr;
		}
	} else {
		std::vector<char> replaced;
		auto replaced_bi = std::back_inserter(replaced);
		utf8::replace_invalid(in_buf, in_buf_end, replaced_bi, invalid_char_replacement);
		utf8::unchecked::utf8to16(replaced.begin(), replaced.end(), res_bi);
		if (first_invalid_char != nullptr) {
			*first_invalid_char = first_invalid_found;
		}
	}

	res.push_back(0);
	return SqlWString(std::move(res));
}

size_t utf16_length(const SQLWCHAR *buf) {
	if (!buf) {
		return 0;
	}

	const SQLWCHAR *ptr = buf;
	while (*ptr != 0) {
		ptr++;
	}

	return static_cast<size_t>(ptr - buf);
}

} // namespace odbcscanner
