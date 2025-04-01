cmake_minimum_required(VERSION 3.10)
project(surveylib C)

# Set project directories
set(SRC_DIR "src")
set(INCLUDE_DIR "include")
set(TEST_DIR "test")
set(TOOLS_DIR "tools")

# Find SQLite3
find_package(SQLite3 REQUIRED)

# Library sources
set(SOURCES
    ${SRC_DIR}/cavecarto.c
    ${SRC_DIR}/igrf.c
    ${SRC_DIR}/matrix_builder.c
    ${SRC_DIR}/surveyfile.c
    ${SRC_DIR}/csparse.c
)

set(HEADERS
    ${INCLUDE_DIR}/surveyfile.h
    ${INCLUDE_DIR}/cavecarto.h
)

# Create the library
add_library(surveylib ${SOURCES} ${HEADERS})
target_include_directories(surveylib PUBLIC ${INCLUDE_DIR} PRIVATE ${SRC_DIR})
target_link_libraries(surveylib PRIVATE SQLite::SQLite3)

# Tools
add_executable(surveyclose ${TOOLS_DIR}/surveyclose.c)
target_link_libraries(surveyclose PRIVATE surveylib)

# Tests
add_executable(test_suite ${TEST_DIR}/test.c)
target_include_directories(test_suite PRIVATE ${SRC_DIR})
target_link_libraries(test_suite PRIVATE surveylib)

add_executable(bench_suite ${TEST_DIR}/bench.c)
target_include_directories(bench_suite PRIVATE ${SRC_DIR})
target_link_libraries(bench_suite PRIVATE surveylib)

# Enable testing
enable_testing()
add_test(NAME test_suite COMMAND test_suite)
