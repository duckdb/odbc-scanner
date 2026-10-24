cmake_minimum_required(VERSION 3.5...3.29)

if (MSVC) 
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR})
endif ()

add_executable(test_${EXTENSION_NAME}
    ../duckdb_capi_v2/cpp/duckdb_cpp.cpp
    test_basic.cpp
    test_cancel.cpp
    test_common.cpp
    test_copy.cpp
    test_dates.cpp
    test_decimal.cpp
    test_list_env.cpp
    test_long_strings.cpp
    test_params.cpp
    test_version.cpp
)

if (MSVC)
    # C++ API sources: integer conversions and local variables shadowing class members
    set_source_files_properties(../duckdb_capi_v2/cpp/duckdb_cpp.cpp PROPERTIES COMPILE_OPTIONS "/wd4244;/wd4458")
endif ()

target_include_directories(test_${EXTENSION_NAME} PRIVATE
    ${ODBC_INCLUDE_DIRS}
    ../third_party/catch
    ../duckdb_capi_v2
    ../duckdb_capi_v2/cpp
)

string(TOLOWER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_LOWER)

target_compile_definitions(test_${EXTENSION_NAME} PRIVATE
    -DODBC_SCANNER_EXTENSION_FILE_PATH=${CMAKE_CURRENT_LIST_DIR}/../build/${CMAKE_BUILD_TYPE_LOWER}/${EXTENSION_NAME}.duckdb_extension
)

target_link_libraries(test_${EXTENSION_NAME} PRIVATE
    $ENV{DUCKDB_CAPI_LIB_PATH}
)
