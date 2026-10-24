#include "capi_entry_point.hpp"

#include "duckdb_cpp_extension.hpp"

DUCKDB_CPP_EXTENSION_ENTRYPOINT(duckdb::cxx::Extension &extension, duckdb::cxx::Context &context) {
	initiaize_odbc_scanner(extension, context);
}
