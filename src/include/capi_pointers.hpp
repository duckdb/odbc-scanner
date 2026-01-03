#pragma once

#include <memory>

#include "duckdb_extension_api.hpp"

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

using ConfigPtr = std::unique_ptr<_duckdb_config, void (*)(duckdb_config)>;

inline void ConfigDeleter(duckdb_config config) {
	duckdb_destroy_config(&config);
}

using DatabasePtr = std::unique_ptr<_duckdb_database, void (*)(duckdb_database)>;

inline void DatabaseDeleter(duckdb_database db) {
	duckdb_close(&db);
}

using ConnectionPtr = std::unique_ptr<_duckdb_connection, void (*)(duckdb_connection)>;

inline void ConnectionDeleter(duckdb_connection conn) {
	duckdb_disconnect(&conn);
}

using ResultPtr = std::unique_ptr<duckdb_result, void (*)(duckdb_result *)>;

inline void ResultDeleter(duckdb_result *res) {
	duckdb_destroy_result(res);
}

using DataChunkPtr = std::unique_ptr<_duckdb_data_chunk, void (*)(duckdb_data_chunk)>;

inline void DataChunkDeleter(duckdb_data_chunk chunk) {
	duckdb_destroy_data_chunk(&chunk);
}

} // namespace odbcscanner
