#pragma once

#include <stdexcept>
#include <string>

namespace odbcscanner {

class ScannerException : public std::exception {
protected:
	std::string message;

public:
	ScannerException() = default;

	ScannerException(const std::string &message) : message(message.data(), message.length()) {
	}

	virtual const char *what() const noexcept {
		return message.c_str();
	}
};

} // namespace odbcscanner