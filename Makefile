.PHONY: clean clean_all format

PROJ_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Main extension configuration
EXTENSION_NAME := odbc_scanner

# Set to 1 to enable Unstable API (binaries will only work on TARGET_DUCKDB_VERSION, forwards compatibility will be broken)
# WARNING: When set to 1, the duckdb_extension.h from the TARGET_DUCKDB_VERSION must be used, using any other version of
#          the header is unsafe.
USE_UNSTABLE_C_API := 0

# The DuckDB version to target
TARGET_DUCKDB_VERSION := v1.2.0

# Detect platform
PLATFORM := $(shell uname -s)
ifeq ($(PLATFORM),Linux)
	DUCKDB_ODBC_LIB_NAME := libduckdb_odbc.so
	# detect amd64 or arm64 architecture
	ifeq ($(shell uname -m),x86_64)
		RELEASE_BUILD = linux-amd64
	else ifeq ($(shell uname -m),aarch64)
		RELEASE_BUILD = linux-arm64
	endif
else ifeq ($(PLATFORM),Darwin)
	DUCKDB_ODBC_LIB_NAME := libduckdb_odbc.dylib
	RELEASE_BUILD = osx-universal
else ifeq ($(PLATFORM),Windows_NT)
	DUCKDB_ODBC_LIB_NAME := duckdb_odbc.dll
	RELEASE_BUILD = windows-amd64
else
	$(error Unsupported platform: $(PLATFORM))
endif

ENABLE_C_API_TESTS := TRUE
DUCKDB_ODBC_FINAL_PATH :=

# Check if the ODBC library is available
ifneq ("$(wildcard $(PROJ_DIR)/../duckdb-odbc/build/debug/$(DUCKDB_ODBC_LIB_NAME))","")
	DUCKDB_ODBC_FINAL_PATH := $(PROJ_DIR)/../duckdb-odbc/build/debug/$(DUCKDB_ODBC_LIB_NAME)
else ifneq ("$(wildcard $(PROJ_DIR)/third_party/duckdb-odbc/$(DUCKDB_ODBC_LIB_NAME))","")
	DUCKDB_ODBC_FINAL_PATH := $(PROJ_DIR)/third_party/duckdb-odbc/$(DUCKDB_ODBC_LIB_NAME)
else
	ENABLE_C_API_TESTS := FALSE
endif

CMAKE_EXTRA_BUILD_FLAGS := -DDUCKDB_ODBC_SHARED_LIB_PATH=$(DUCKDB_ODBC_FINAL_PATH) -DENABLE_C_API_TESTS=$(ENABLE_C_API_TESTS)

all: configure release

# Include makefiles from DuckDB
include extension-ci-tools/makefiles/c_api_extensions/base.Makefile
include extension-ci-tools/makefiles/c_api_extensions/c_cpp.Makefile

configure: venv platform extension_version

debug: build_extension_library_debug build_extension_with_metadata_debug
release: build_extension_library_release build_extension_with_metadata_release

test: debug test_sql test_c_api

test_sql: debug test_extension_debug

test_c_api: debug
ifeq ($(ENABLE_C_API_TESTS), TRUE)
	@echo "Running C API tests using DuckDB ODBC library at $(DUCKDB_ODBC_FINAL_PATH)"
	./cmake_build/debug/test/test_odbc_scanner
else
	@echo "C API tests are disabled, DuckDB ODBC library not found."
	@echo "Run Clone duckdb-odbc repository and run 'make debug' to build it."
	exit 1
endif

clean: clean_build clean_cmake
clean_all: clean clean_configure

format: format-fix

format-fix:
	python resources/scripts/format.py --all --fix --noconfirm

format-check:
	python resources/scripts/format.py --check