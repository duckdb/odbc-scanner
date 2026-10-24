#pragma once

#include "duckdb_cpp.hpp"

using idx_t = duckdb::cxx::idx_t;

namespace odbcscanner {

using duckdb::cxx::blob_t;
using duckdb::cxx::Connection;
using duckdb::cxx::Context;
using duckdb::cxx::DataChunk;
using duckdb::cxx::Environment;
using duckdb::cxx::Extension;
using duckdb::cxx::Factory;
using duckdb::cxx::FunctionBindInput;
using duckdb::cxx::FunctionParameterKind;
using duckdb::cxx::FunctionSignature;
using duckdb::cxx::Instance;
using duckdb::cxx::int128_t;
using duckdb::cxx::list_entry_t;
using duckdb::cxx::LogicalType;
using duckdb::cxx::LogicalTypeId;
using duckdb::cxx::QueryResult;
using duckdb::cxx::ScalarFunction;
using duckdb::cxx::Schema;
using duckdb::cxx::SqlStatement;
using duckdb::cxx::TableFunction;
using duckdb::cxx::TypeParam;
using duckdb::cxx::uint128_t;
using duckdb::cxx::Value;
using duckdb::cxx::varchar_t;
using duckdb::cxx::Vector;
using duckdb::cxx::VectorView;

} // namespace odbcscanner
