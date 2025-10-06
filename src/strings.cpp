#include "strings.hpp"

#include <algorithm>
#include <sstream>

namespace odbcscanner {

bool Strings::IsSpace(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

std::string Strings::Trim(const std::string &str) {
	auto front = std::find_if_not(str.begin(), str.end(), IsSpace);
	auto back = std::find_if_not(str.rbegin(), str.rend(), IsSpace).base();
	return (back <= front ? std::string() : std::string(front, back));
}

std::vector<std::string> Strings::Split(const std::string &str, char delim) {
	std::stringstream ss(str);
	std::vector<std::string> res;
	std::string item;
	while (std::getline(ss, item, delim)) {
		std::string trimmed = Strings::Trim(item);
		if (!trimmed.empty()) {
			res.push_back(trimmed);
		}
	}
	return res;
}

} // namespace odbcscanner