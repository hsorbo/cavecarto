# libcavecarto/CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

file(GLOB LIBCAVECARTO_SOURCES src/*.c)
file(GLOB LIBCAVECARTO_HEADERS include/*.h)

add_library(libcavecarto ${LIBCAVECARTO_SOURCES} ${LIBCAVECARTO_HEADERS})
target_include_directories(libcavecarto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)