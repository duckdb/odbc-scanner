#pragma once

#include <string>
#include <vector>

namespace odbcscanner {

struct Strings {

	static bool IsSpace(char c);

	static std::string Trim(const std::string &str);

	static std::vector<std::string> Split(const std::string &str, char delim);

	static std::string ReplaceAll(std::string &str, const std::string &snippet, const std::string &replacement);

	static std::string ToUpper(const std::string &str);
};

} // namespace odbcscanner
