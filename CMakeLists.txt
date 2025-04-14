cmake_minimum_required(VERSION 3.10)
project(surveylib C)

# Set project directories
set(SRC_DIR "src")
set(INCLUDE_DIR "include")
set(LIBCAVECARTO_DIR "libcavecarto")
set(LIBCAVECARTO_SRC_DIR "${LIBCAVECARTO_DIR}/src")
set(LIBCAVECARTO_INCLUDE_DIR "${LIBCAVECARTO_DIR}/include")
set(TEST_DIR "test")
set(TOOLS_DIR "tools")

# Find SQLite3
find_package(SQLite3 REQUIRED)

# === libcavecarto ===
file(GLOB LIBCAVECARTO_SOURCES
    ${LIBCAVECARTO_SRC_DIR}/*.c
)

file(GLOB LIBCAVECARTO_HEADERS
    ${LIBCAVECARTO_INCLUDE_DIR}/*.h
)

add_library(libcavecarto ${LIBCAVECARTO_SOURCES} ${LIBCAVECARTO_HEADERS})
target_include_directories(libcavecarto PUBLIC ${LIBCAVECARTO_INCLUDE_DIR})

# === surveylib ===
set(SURVEY_SOURCES
    ${SRC_DIR}/surveyfile.c
)

set(SURVEY_HEADERS
    ${INCLUDE_DIR}/surveyfile.h
)

add_library(surveylib ${SURVEY_SOURCES} ${SURVEY_HEADERS})
target_include_directories(surveylib PUBLIC ${INCLUDE_DIR} PRIVATE ${SRC_DIR})
target_link_libraries(surveylib PRIVATE libcavecarto SQLite::SQLite3)

# Tools
add_executable(surveyclose ${TOOLS_DIR}/surveyclose.c)
target_link_libraries(surveyclose PRIVATE surveylib libcavecarto)

# Tests
add_executable(test_suite ${TEST_DIR}/test.c)
target_include_directories(test_suite PRIVATE ${SRC_DIR} ${LIBCAVECARTO_SRC_DIR})
target_link_libraries(test_suite PRIVATE surveylib libcavecarto)

add_executable(bench_suite ${TEST_DIR}/bench.c)
target_include_directories(bench_suite PRIVATE ${SRC_DIR} ${LIBCAVECARTO_SRC_DIR})
target_link_libraries(bench_suite PRIVATE surveylib libcavecarto)

# Enable testing
enable_testing()
add_test(NAME test_suite COMMAND test_suite)
