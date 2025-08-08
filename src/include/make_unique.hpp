
#pragma once

#include <memory>

namespace odbcscanner {

template <typename T, typename... Args>
std::unique_ptr<T> std_make_unique(Args &&...args) {
	return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

} // namespace odbcscanner
