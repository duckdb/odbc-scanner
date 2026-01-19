cmake_minimum_required(VERSION 3.5...3.29)
option(DUCKDB_WASM_EXTENSIONS "Whether compiling for Wasm target" OFF)

###
# Configuration
###

# CMake-only build config
if(DEFINED DUCKDB_MODULE_BASE_DIR) 
    set(EXTENSION_NAME odbc_scanner)
    set(${EXTENSION_NAME}_CMAKE_ONLY_BUILD TRUE)

    set(TARGET_DUCKDB_VERSION_MAJOR 1)
    set(TARGET_DUCKDB_VERSION_MINOR 2)
    set(TARGET_DUCKDB_VERSION_PATCH 0)

    find_package(Git)
    if(Git_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short=10 HEAD
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE ${EXTENSION_NAME}_GIT_COMMIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE)
    else()
        message(FATAL_ERROR "Unable to get Git version for extension: ${EXTENSION_NAME}")
    endif()
endif()
# end CMake-only build config

if(NOT DEFINED EXTENSION_NAME)
    message(FATAL_ERROR "DuckDB extension name is required")
endif()
if (DEFINED TARGET_DUCKDB_VERSION_MAJOR)
    add_definitions(-DDUCKDB_EXTENSION_API_VERSION_MAJOR=${TARGET_DUCKDB_VERSION_MAJOR})
endif()
if (DEFINED TARGET_DUCKDB_VERSION_MINOR)
    add_definitions(-DDUCKDB_EXTENSION_API_VERSION_MINOR=${TARGET_DUCKDB_VERSION_MINOR})
endif()
if (DEFINED TARGET_DUCKDB_VERSION_PATCH)
    add_definitions(-DDUCKDB_EXTENSION_API_VERSION_PATCH=${TARGET_DUCKDB_VERSION_PATCH})
endif()

add_definitions(-DDUCKDB_EXTENSION_NAME=${EXTENSION_NAME})

if (DEFINED DUCKDB_EXTENSION_API_VERSION_UNSTABLE)
    add_definitions(-DDUCKDB_EXTENSION_API_VERSION_UNSTABLE=${DUCKDB_EXTENSION_API_VERSION_UNSTABLE})
endif()

###
# Build
###
project(${EXTENSION_NAME} LANGUAGES C CXX)

find_package(ODBC REQUIRED)
message(STATUS "ODBC library found at: " ${ODBC_LIBRARIES})

if (NOT MSVC)
    get_filename_component(ODBC_LIB_DIR ${ODBC_LIBRARIES} DIRECTORY)
    find_library(ODBCINST_LIB NAMES odbcinst libodbcinst REQUIRED HINTS ${ODBC_LIB_DIR})
endif()

set(CMAKE_CXX_STANDARD "11" CACHE STRING "C++ standard to enforce")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(MSVC) 
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR})
endif()

# Create Extension library
set(EXTENSION_SOURCES
    src/binary.cpp
    src/capi_entry_point.cpp
    src/columns.cpp
    src/connection.cpp
    src/dbms_quirks.cpp
    src/diagnostics.cpp
    src/odbc_scanner.cpp
    src/params.cpp
    src/registries.cpp
    src/scanner_value.cpp
    src/strings.cpp
    src/widechar.cpp
    src/functions/odbc_begin_transaction.cpp
    src/functions/odbc_commit.cpp
    src/functions/odbc_bind_params.cpp
    src/functions/odbc_close.cpp
    src/functions/odbc_connect.cpp
    src/functions/odbc_copy_from.cpp
    src/functions/odbc_create_params.cpp
    src/functions/odbc_list_data_sources.cpp
    src/functions/odbc_list_drivers.cpp
    src/functions/odbc_query.cpp
    src/functions/odbc_rollback.cpp
    src/mappings/generic_mapping.cpp
    src/mappings/mappings.cpp
    src/mappings/oracle_mapping.cpp
    src/types/blob_type.cpp
    src/types/bool_type.cpp
    src/types/decimal_type.cpp
    src/types/float_types.cpp
    src/types/integer_types.cpp
    src/types/null_type.cpp
    src/types/temporal_types.cpp
    src/types/types.cpp
    src/types/uuid_type.cpp
    src/types/varchar_type.cpp
)

add_library(${EXTENSION_NAME} SHARED ${EXTENSION_SOURCES})

if(DEFINED ${EXTENSION_NAME}_CMAKE_ONLY_BUILD)
    add_custom_command(
        TARGET odbc_scanner
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:${EXTENSION_NAME}>
            ${CMAKE_CURRENT_BINARY_DIR}/${EXTENSION_NAME}.duckdb_extension
        COMMAND ${CMAKE_COMMAND}
            -DABI_TYPE=C_STRUCT
            -DEXTENSION=${CMAKE_CURRENT_BINARY_DIR}/odbc_scanner.duckdb_extension
            -DPLATFORM_FILE=${DuckDB_BINARY_DIR}/duckdb_platform_out
            -DVERSION_FIELD=v${TARGET_DUCKDB_VERSION_MAJOR}.${TARGET_DUCKDB_VERSION_MINOR}.${TARGET_DUCKDB_VERSION_PATCH}
            -DEXTENSION_VERSION=${${EXTENSION_NAME}_GIT_COMMIT_HASH}
            -DNULL_FILE=${DUCKDB_MODULE_BASE_DIR}/scripts/null.txt
            -P ${DUCKDB_MODULE_BASE_DIR}/scripts/append_metadata.cmake
        DEPENDS duckdb_platform
    )
endif()

# Hide symbols
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(VISIBILITY_INLINES_HIDDEN ON)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
elseif (APPLE)
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,-x")
endif()

target_include_directories(${EXTENSION_NAME} PRIVATE
    third_party/utf8cpp
    duckdb_capi
    ${ODBC_INCLUDE_DIRS}
    src/include
)

if(NOT MSVC)
    target_compile_options(${EXTENSION_NAME} PRIVATE
        -Wall
        -Werror
        -Wextra
    )
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(${EXTENSION_NAME} PRIVATE
            -O0
        )
    endif()
else()
    target_compile_options(${EXTENSION_NAME} PRIVATE
        /W4
        /WX
    )
    target_compile_definitions(${EXTENSION_NAME} PRIVATE
        -D_CRT_SECURE_NO_WARNINGS
    )
endif()

target_link_libraries(${EXTENSION_NAME} PRIVATE
  ${ODBC_LIBRARIES}
)

SET(CAPI_ERROR_MSG "C API test suite is disabled, to enable it specify DuckDB shared library in 'DUCKDB_CAPI_LIB_PATH' environment variable.")

if(ENABLE_C_API_TESTS)
    message(STATUS "C API test suite is enabled using shared lib: $ENV{DUCKDB_CAPI_LIB_PATH}")

    if(NOT DEFINED ENV{DUCKDB_CAPI_LIB_PATH} OR NOT EXISTS $ENV{DUCKDB_CAPI_LIB_PATH})
        message(FATAL_ERROR ${CAPI_ERROR_MSG})
    endif()

    add_subdirectory(test)
else()
    message(STATUS ${CAPI_ERROR_MSG})
endif()
