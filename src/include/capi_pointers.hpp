#pragma once

#include "duckdb_extension.h"

#include <memory>

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

using LogicalTypePtr = std::unique_ptr<_duckdb_logical_type, void (*)(duckdb_logical_type)>;

inline void LogicalTypeDeleter(duckdb_logical_type lt) {
	duckdb_destroy_logical_type(&lt);
}

using ScalarFunctionPtr = std::unique_ptr<_duckdb_scalar_function, void (*)(duckdb_scalar_function)>;

inline void ScalarFunctionDeleter(duckdb_scalar_function fun) {
	duckdb_destroy_scalar_function(&fun);
}

using TableFunctionPtr = std::unique_ptr<_duckdb_table_function, void (*)(duckdb_table_function)>;

inline void TableFunctionDeleter(duckdb_table_function fun) {
	duckdb_destroy_table_function(&fun);
}

using ValuePtr = std::unique_ptr<_duckdb_value, void (*)(duckdb_value)>;

inline void ValueDeleter(duckdb_value val) {
	duckdb_destroy_value(&val);
}

using VarcharPtr = std::unique_ptr<char, void (*)(char *)>;

inline void VarcharDeleter(char *val) {
	duckdb_free(val);
}

} // namespace odbcscanner
