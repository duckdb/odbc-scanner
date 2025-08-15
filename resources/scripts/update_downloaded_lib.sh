#!/bin/bash

# This script accepts the platform specific
if [ -z "$1" ]; then
  echo "Usage: $0 <PLATFORM_SPECIFIC_LIBRARY_NAME>"
  echo "Example on MacOS: $0 macOS osx-universal"
  echo "Example on Linux: $0 linux linux-amd64"
  exit 1
fi

PLATFORM="$1"

# Check if the library directory exists
PROJ_DIR=$(pwd)
if [ -d "$PROJ_DIR/third_party/duckdb-odbc" ]; then
  echo "Directory third_party/duckdb-odbc already exists."
  echo "Removing the directory."
  rm -rf "$PROJ_DIR/third_party/duckdb-odbc"
fi
  mkdir -p "$PROJ_DIR/third_party/duckdb-odbc"

# download the latest duckdb-odbc library
echo "Downloading DuckDB ODBC library for platform: $PLATFORM"
curl -L "https://github.com/duckdb/duckdb-odbc/releases/latest/download/duckdb_odbc-$PLATFORM.zip" -o "$PROJ_DIR/third_party/duckdb-odbc.zip"

# Unzip the downloaded file into the third_party directory and remove the zip file
unzip -o "$PROJ_DIR/third_party/duckdb-odbc.zip" -d "$PROJ_DIR/third_party/duckdb-odbc"
rm "$PROJ_DIR/third_party/duckdb-odbc.zip"

# The library name is the file inside the unzipped directory
LIBRARY_FILE=$(find third_party/duckdb-odbc -name "*.so" -o -name "*.dylib" | head -1 | xargs basename)
if [ -z "$LIBRARY_FILE" ]; then
  echo "No library file found in the unzipped directory."
  exit 1
else
  echo "Found library file: $LIBRARY_FILE"
fi

# Check if unixODBC is installed
if command -v odbcinst >/dev/null 2>&1 && command -v isql >/dev/null 2>&1; then
  echo "unixODBC is already installed."
else
  echo "unixODBC is not installed, install it using your package manager."
fi

# Navigate to the directory where the DuckDB ODBC library is located
cd third_party/duckdb-odbc/ || exit

echo "Checking current library dependencies..."

# Platform-specific dependency checking and fixing
if [[ "$PLATFORM" = *"osx"*  || "$PLATFORM" = *"macos"* ]]; then
  otool -L "$LIBRARY_FILE"

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

  echo "Using Homebrew prefix: $HOMEBREW_PREFIX"

  # Update the library paths to point to the Homebrew-installed libraries
  install_name_tool -change \
      /Users/runner/work/duckdb-odbc/duckdb-odbc/build/unixodbc/build/lib/libodbcinst.2.dylib \
      "$HOMEBREW_PREFIX/lib/libodbcinst.2.dylib" \
      "$LIBRARY_FILE"

  install_name_tool -change \
      /Users/runner/work/duckdb-odbc/duckdb-odbc/build/unixodbc/build/lib/libodbc.2.dylib \
      "$HOMEBREW_PREFIX/lib/libodbc.2.dylib" \
      "$LIBRARY_FILE"

  echo "Verifying updated library dependencies..."
  otool -L "$LIBRARY_FILE"

elif [[ "$PLATFORM" = *"linux"* ]]; then
  # Linux-specific operations
  ldd "$LIBRARY_FILE" || echo "Library dependencies check completed"

  # On Linux, the shared libraries should automatically find system libraries
  echo "Linux library setup completed. System libraries should be automatically detected."
fi

echo "Library setup completed for $PLATFORM"

# Navigate back to the root directory
cd ../..