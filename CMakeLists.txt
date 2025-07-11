cmake_minimum_required(VERSION 3.10)
project(Cpp_playground VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CPP_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)
SET(src_include_dir ${PROJECT_SOURCE_DIR}/src/include)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -ftree-vectorize")



include_directories(${src_include_dir})
add_executable(main src/main.cpp)

find_package(fmt)
target_link_libraries(main fmt::fmt)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

