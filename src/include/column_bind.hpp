#pragma once

#include <string>

#include "odbc_api.hpp"
#include "scanner_value.hpp"

namespace odbcscanner {

struct ColumnBind {
	ScannerValue value;
	SQLLEN ind = 0;

	ColumnBind() {
	}

	ColumnBind(ScannerValue value_in) : value(std::move(value_in)) {
	}

	ColumnBind(const ColumnBind &other) = delete;
	ColumnBind(ColumnBind &&other) = default;

	ColumnBind &operator=(const ColumnBind &other) = delete;
	ColumnBind &operator=(ColumnBind &&other) = default;

	template <typename T>
	T &Value() {
		return value.Value<T>();
	}

	SQLLEN &Indicator() {
		return ind;
	}
};

} // namespace odbcscanner
