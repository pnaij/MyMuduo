cmake_minimum_required(VERSION 3.28.3)
project(muduoSelf)

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -g -std=c++11 -fPIC")

set(LIBRARY_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/lib)
include_directories(${PROJECT_SOURCE_DIR}/include)

add_subdirectory(src)
add_subdirectory(example)

#add_library(muduoSelf SHARED ${SRC_LIST})