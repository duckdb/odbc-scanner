.PHONY: clean clean_all format format-fix format-check test test_debug test_c_api_debug test_all test_all_debug

PROJ_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Main extension configuration
EXTENSION_NAME := odbc_scanner

# Set to 1 to enable Unstable API (binaries will only work on TARGET_DUCKDB_VERSION, forwards compatibility will be broken)
# WARNING: When set to 1, the duckdb_extension.h from the TARGET_DUCKDB_VERSION must be used, using any other version of
#          the header is unsafe.
USE_UNSTABLE_C_API := 0

# The DuckDB version to target
TARGET_DUCKDB_VERSION := v1.2.0

ENABLE_C_API_TESTS := TRUE

# Check if the ODBC library is available
ifeq ("$(DUCKDB_CAPI_LIB_PATH)","")
	ENABLE_C_API_TESTS := FALSE
endif

CMAKE_EXTRA_BUILD_FLAGS := -DENABLE_C_API_TESTS=$(ENABLE_C_API_TESTS)

CAPI_RUN_MSG := "Running C API tests using shared lib at $(DUCKDB_CAPI_LIB_PATH)"
CAPI_ERROR_MSG := "C API test suite is disabled, to enable it specify DuckDB shared library in 'DUCKDB_CAPI_LIB_PATH' environment variable."

all: configure release

# Include makefiles from DuckDB
include extension-ci-tools/makefiles/c_api_extensions/base.Makefile
include extension-ci-tools/makefiles/c_api_extensions/c_cpp.Makefile

configure: venv platform extension_version

debug: build_extension_library_debug build_extension_with_metadata_debug

#build_extension_with_metadata_release: build_extension_library_release 

release: build_extension_library_release build_extension_with_metadata_release

test_c_api:
ifeq ($(ENABLE_C_API_TESTS), TRUE)
	@echo "$(CAPI_RUN_MSG)"
	./cmake_build/release/test/test_odbc_scanner
else
	@echo "$(CAPI_ERROR_MSG)"
	exit 0
endif

test_c_api_debug:
ifeq ($(ENABLE_C_API_TESTS), TRUE)
	@echo "$(CAPI_RUN_MSG)"
	./cmake_build/debug/test/test_odbc_scanner
else
	@echo "$(CAPI_ERROR_MSG)"
	exit 0
endif

test: test_c_api

test_release: test_c_api

test_debug: test_c_api_debug

clean: clean_build clean_cmake

clean_all: clean clean_configure

format: format-fix

format-fix:
	python resources/scripts/format.py

format-check:
	python resources/scripts/format.py --check
