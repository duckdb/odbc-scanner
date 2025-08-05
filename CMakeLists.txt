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
get_filename_component(ODBC_LIB_DIR ${ODBC_LIBRARIES} DIRECTORY)
find_library(ODBCINST_LIB NAMES odbcinst libodbcinst REQUIRED HINTS ${ODBC_LIB_DIR})

set(CMAKE_CXX_STANDARD "11" CACHE STRING "C++ standard to enforce")

# Create Extension library
set(EXTENSION_SOURCES
    src/capi_odbc_scanner.c
    src/common.cpp
    src/fetch.cpp
    src/odbc_close.cpp
    src/odbc_connect.cpp
    src/odbc_connection.cpp
    src/odbc_params.cpp
    src/odbc_query.cpp
    src/params.cpp
    src/types.cpp
    src/widechar.cpp
)

if (DUCKDB_WASM_EXTENSION)
	add_library(${EXTENSION_NAME} STATIC ${EXTENSION_SOURCES})
else()
	add_library(${EXTENSION_NAME} SHARED ${EXTENSION_SOURCES})
endif()

# Hide symbols
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")

target_include_directories(${EXTENSION_NAME} PRIVATE
    third_party/utf8cpp
    duckdb_capi
    ${ODBC_INCLUDE_DIRS}
    src/include
)

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

target_link_libraries(${EXTENSION_NAME} PRIVATE
  ${ODBC_LIBRARIES}
)

add_subdirectory(test)
