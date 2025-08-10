#pragma once

#include <utility>

namespace odbcscanner {

template <typename T>
class DeferGuard {
	T func;
	bool moved_out = false;

public:
	explicit DeferGuard(T func) : func(std::move(func)) {
	}

	DeferGuard(const DeferGuard &) = delete;
	DeferGuard &operator=(const DeferGuard &) = delete;

	DeferGuard(DeferGuard &&other) noexcept : func(std::move(other.func)) {
		other.moved_out = true;
	}

	DeferGuard &operator=(DeferGuard &&) = delete;

	~DeferGuard() {
		if (!moved_out) {
			func();
		}
	}
};

template <typename T>
DeferGuard<T> Defer(T func) {
	return DeferGuard<T>(std::move(func));
}

} // namespace odbcscanner
