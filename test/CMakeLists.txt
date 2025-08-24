add_executable(test_${EXTENSION_NAME}
    test_basic.cpp
    test_common.cpp
    test_long_strings.cpp
    test_params.cpp
    test_dates.cpp
)

target_include_directories(test_${EXTENSION_NAME} PRIVATE
    ${ODBC_INCLUDE_DIRS}
    ../third_party/catch
    ../duckdb_capi
)

string(TOLOWER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_LOWER)

target_compile_definitions(test_${EXTENSION_NAME} PRIVATE
    -DODBC_SCANNER_EXTENSION_FILE_PATH=${CMAKE_CURRENT_LIST_DIR}/../build/${CMAKE_BUILD_TYPE_LOWER}/${EXTENSION_NAME}.duckdb_extension
)

target_link_libraries(test_${EXTENSION_NAME} PRIVATE
    $ENV{DUCKDB_ODBC_SHARED_LIB_PATH}
)
