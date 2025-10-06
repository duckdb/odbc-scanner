#pragma once

#include <string>
#include <vector>

namespace odbcscanner {

struct Strings {

	static bool IsSpace(char c);

	static std::string Trim(const std::string &str);

	static std::vector<std::string> Split(const std::string &str, char delim);
};

} // namespace odbcscanner
