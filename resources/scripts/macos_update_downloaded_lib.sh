#!/bin/bash

set -x
set -e

# This script accepts the ODBC shared lib path 
if [ -z "$1" ]; then
  echo "Usage: $0 path/to/libduckdb_odbc.dylib"
  exit 1
fi

LIBDUCKDB_ODBC_DYLIB="$1"

otool -L "${LIBDUCKDB_ODBC_DYLIB}"

# Detect Homebrew installation path
if [ -d "/opt/homebrew" ]; then
  # Apple Silicon Mac
  HOMEBREW_PREFIX="/opt/homebrew"
elif [ -d "/usr/local" ]; then
  # Intel Mac
  HOMEBREW_PREFIX="/usr/local"
else
  echo "Cannot find Homebrew installation path"
  exit 1
fi

echo "Using Homebrew prefix: ${HOMEBREW_PREFIX}"

# Update the library paths to point to the Homebrew-installed libraries
install_name_tool -change \
    /Users/runner/work/duckdb-odbc/duckdb-odbc/build/unixodbc/build/lib/libodbcinst.2.dylib \
    "${HOMEBREW_PREFIX}/lib/libodbcinst.2.dylib" \
    "${LIBDUCKDB_ODBC_DYLIB}"

install_name_tool -change \
    /Users/runner/work/duckdb-odbc/duckdb-odbc/build/unixodbc/build/lib/libodbc.2.dylib \
    "${HOMEBREW_PREFIX}/lib/libodbc.2.dylib" \
    "${LIBDUCKDB_ODBC_DYLIB}"

echo "Verifying updated library dependencies..."
otool -L "${LIBDUCKDB_ODBC_DYLIB}"
