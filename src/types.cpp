#include "types.hpp"

#include "scanner_exception.hpp"

#include <string>

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

duckdb_type OdbcCTypeToDuckType(SQLLEN odbc_ctype) {
	switch (odbc_ctype) {
	case SQL_INTEGER:
		return DUCKDB_TYPE_BIGINT;
	case SQL_VARCHAR:
		return DUCKDB_TYPE_VARCHAR;
	default:
		throw ScannerException("Unsupported ODBC C type: " + std::to_string(odbc_ctype));
	}
}

} // namespace odbcscanner
