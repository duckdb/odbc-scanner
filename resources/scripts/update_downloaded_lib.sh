#!/bin/bash

# This script accepts the platform specific
if [ -z "$1" ]; then
  echo "Usage: $0 <PLATFORM_SPECIFIC_LIBRARY_NAME>"
  echo "Example on MacOS: $0 osx-universal"
  exit 1
fi

RELEASE_BUILD="$1"

# Check if the library directory exists
PROJ_DIR=$(pwd)
if [ -d "$PROJ_DIR/third_party/duckdb-odbc" ]; then
  echo "Directory third_party/duckdb-odbc already exists."
  echo "Removing the directory."
  rm -rf "$PROJ_DIR/third_party/duckdb-odbc"
fi
  mkdir -p "$PROJ_DIR/third_party/duckdb-odbc"

# download the latest duckdb-odbc library
PLATFORM_SPECIFIC_LIBRARY_NAME="$1"
echo "Downloading DuckDB ODBC library for platform: $PLATFORM_SPECIFIC_LIBRARY_NAME"
curl -L "https://github.com/duckdb/duckdb-odbc/releases/latest/download/duckdb_odbc-$RELEASE_BUILD.zip" -o "$PROJ_DIR/third_party/duckdb-odbc.zip"

# Unzip the downloaded file into the third_party directory and remove the zip file
unzip -o "$PROJ_DIR/third_party/duckdb-odbc.zip" -d "$PROJ_DIR/third_party/duckdb-odbc"
rm "$PROJ_DIR/third_party/duckdb-odbc.zip"

# The library name is the file inside the unzipped directory
LIBRARY_FILE=$(ls third_party/duckdb-odbc)
if [ -z "$LIBRARY_FILE" ]; then
  echo "No library file found in the unzipped directory."
  exit 1
else
  echo "Found library file: $LIBRARY_FILE"
fi

# Install unixODBC if not already installed
brew install unixodbc

# Navigate to the directory where the DuckDB ODBC library is located
cd third_party/duckdb-odbc/ || exit

echo "Checking current library dependencies..."
otool -L "$LIBRARY_FILE"

# Update the library paths to point to the Homebrew-installed libraries
install_name_tool -change \
    /Users/runner/work/duckdb-odbc/duckdb-odbc/build/unixodbc/build/lib/libodbcinst.2.dylib \
    /opt/homebrew/lib/libodbcinst.2.dylib \
    "$LIBRARY_FILE"

install_name_tool -change \
    /Users/runner/work/duckdb-odbc/duckdb-odbc/build/unixodbc/build/lib/libodbc.2.dylib \
    /opt/homebrew/lib/libodbc.2.dylib \
    "$LIBRARY_FILE"

echo "Verifying updated library dependencies..."
otool -L libduckdb_odbc.dylib

# Navigate back to the root directory
cd ../..
