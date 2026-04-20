cmake_minimum_required(VERSION 3.15)
project(surveylib C)

# Force C99
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(MSVC)
  add_compile_options(/W3)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
  add_compile_options(-Wall -Werror)
endif()

# Set project directories
set(SRC_DIR "src")
set(INCLUDE_DIR "include")
set(TEST_DIR "test")

# Library sources
set(SOURCES
    ${SRC_DIR}/cavecarto.c
    ${SRC_DIR}/igrf.c
    ${SRC_DIR}/dok_builder.c
    ${SRC_DIR}/csparse.c
)

set(HEADERS
    ${INCLUDE_DIR}/cavecarto.h
)

# Create the library
add_library(surveylib ${SOURCES} ${HEADERS})
target_include_directories(surveylib PUBLIC ${INCLUDE_DIR} PRIVATE ${SRC_DIR})
if(NOT WIN32)
  target_link_libraries(surveylib PRIVATE m)
endif()
if(APPLE)
  find_library(ACCELERATE_FRAMEWORK Accelerate)
  if(ACCELERATE_FRAMEWORK)
    target_link_libraries(surveylib PRIVATE "${ACCELERATE_FRAMEWORK}")
    target_compile_definitions(surveylib PRIVATE USE_ACCELERATE)
    message(STATUS "Accelerate found: enabling USE_ACCELERATE")
  else()
    message(STATUS "Accelerate not found: building without USE_ACCELERATE")
  endif()
endif()

# Python bindings
find_package(Python3 COMPONENTS Interpreter Development.Module REQUIRED)

Python3_add_library(surveylib_py MODULE WITH_SOABI python/surveylib_py.c)
target_link_libraries(surveylib_py PRIVATE surveylib)
if(NOT WIN32)
  target_link_libraries(surveylib_py PRIVATE m)
endif()
target_include_directories(surveylib_py PRIVATE ${INCLUDE_DIR} ${SRC_DIR})
set_target_properties(surveylib_py PROPERTIES
    OUTPUT_NAME "_cavecarto"
    PREFIX ""  # Don't prefix with lib
)
install(TARGETS surveylib_py
  LIBRARY DESTINATION cavecarto
  RUNTIME DESTINATION cavecarto
)


# Tests
option(CAVECARTO_BUILD_TESTS "Build native test binaries" ON)
if(CAVECARTO_BUILD_TESTS)
  add_executable(test_suite ${TEST_DIR}/test.c)
  target_include_directories(test_suite PRIVATE ${SRC_DIR})
  target_link_libraries(test_suite PRIVATE surveylib)

  add_executable(bench_suite ${TEST_DIR}/bench.c)
  target_include_directories(bench_suite PRIVATE ${SRC_DIR})
  set_property(TARGET bench_suite PROPERTY C_EXTENSIONS ON)
  target_link_libraries(bench_suite PRIVATE surveylib)

  # Enable testing
  enable_testing()
  add_test(NAME test_suite COMMAND test_suite)
endif()
