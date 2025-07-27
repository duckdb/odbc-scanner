.PHONY: clean clean_all

PROJ_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Main extension configuration
EXTENSION_NAME := odbc_scanner

# Set to 1 to enable Unstable API (binaries will only work on TARGET_DUCKDB_VERSION, forwards compatibility will be broken)
# WARNING: When set to 1, the duckdb_extension.h from the TARGET_DUCKDB_VERSION must be used, using any other version of
#          the header is unsafe.
USE_UNSTABLE_C_API := 0

# The DuckDB version to target
TARGET_DUCKDB_VERSION := v1.2.0

DUCKDB_ODBC_SHARED_LIB_PATH ?= $(PROJ_DIR)/../duckdb-odbc/build/debug/libduckdb_odbc.so

CMAKE_EXTRA_BUILD_FLAGS := -DDUCKDB_ODBC_SHARED_LIB_PATH=$(DUCKDB_ODBC_SHARED_LIB_PATH)

all: configure release

# Include makefiles from DuckDB
include extension-ci-tools/makefiles/c_api_extensions/base.Makefile
include extension-ci-tools/makefiles/c_api_extensions/c_cpp.Makefile

configure: venv platform extension_version

debug: build_extension_library_debug build_extension_with_metadata_debug
release: build_extension_library_release build_extension_with_metadata_release

test: test_debug
test_debug: debug test_extension_debug
	./cmake_build/debug/test/test_odbc_scanner
test_release: release test_extension_release
	./cmake_build/release/test/test_odbc_scanner

clean: clean_build clean_cmake
clean_all: clean clean_configure

format:
	python ./resources/scripts/format.py
