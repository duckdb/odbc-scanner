cmake_minimum_required(VERSION 3.5...3.29)
option(DUCKDB_WASM_EXTENSIONS "Whether compiling for Wasm target" OFF)

###
# Configuration
###
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
endif ()

set(CMAKE_CXX_STANDARD "11" CACHE STRING "C++ standard to enforce")

if (MSVC) 
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR})
endif ()

# Create Extension library
set(EXTENSION_SOURCES
    src/capi_entry_point.cpp
    src/columns.cpp
    src/connection.cpp
    src/dbms_quirks.cpp
    src/diagnostics.cpp
    src/odbc_scanner.cpp
    src/params.cpp
    src/registries.cpp
    src/strings.cpp
    src/widechar.cpp
    src/functions/odbc_bind_params.cpp
    src/functions/odbc_close.cpp
    src/functions/odbc_connect.cpp
    src/functions/odbc_create_params.cpp
    src/functions/odbc_query.cpp
    src/types/blob_type.cpp
    src/types/bool_type.cpp
    src/types/decimal_type.cpp
    src/types/float_types.cpp
    src/types/integer_types.cpp
    src/types/null_type.cpp
    src/types/temporal_types.cpp
    src/types/types.cpp
    src/types/varchar_type.cpp
)

add_library(${EXTENSION_NAME} SHARED ${EXTENSION_SOURCES})

# Hide symbols
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(VISIBILITY_INLINES_HIDDEN ON)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
elseif (APPLE)
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,-x")
endif ()

target_include_directories(${EXTENSION_NAME} PRIVATE
    third_party/utf8cpp
    duckdb_capi
    ${ODBC_INCLUDE_DIRS}
    src/include
)

if (NOT MSVC)
    target_compile_options(${EXTENSION_NAME} PRIVATE
        -Wall
        -Werror
        -Wextra
    )
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(${EXTENSION_NAME} PRIVATE
            -O0
        )
    endif ()
else ()
    target_compile_options(${EXTENSION_NAME} PRIVATE
        /W4
        /WX
    )
    target_compile_definitions(${EXTENSION_NAME} PRIVATE
        -D_CRT_SECURE_NO_WARNINGS
    )
endif ()

target_link_libraries(${EXTENSION_NAME} PRIVATE
  ${ODBC_LIBRARIES}
)

SET (CAPI_ERROR_MSG "C API test suite is disabled, to enable it specify DuckDB shared library in 'DUCKDB_CAPI_LIB_PATH' environment variable.")

if (ENABLE_C_API_TESTS)
    message(STATUS "C API test suite is enabled using shared lib: $ENV{DUCKDB_CAPI_LIB_PATH}")

    if (NOT DEFINED ENV{DUCKDB_CAPI_LIB_PATH} OR NOT EXISTS $ENV{DUCKDB_CAPI_LIB_PATH})
        message(FATAL_ERROR ${CAPI_ERROR_MSG})
    endif ()

    add_subdirectory(test)
else ()
    message(STATUS ${CAPI_ERROR_MSG})
endif ()
