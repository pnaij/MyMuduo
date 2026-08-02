cmake_minimum_required(VERSION 3.10)
project(jpmuduo)

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -g -std=c++11 -fPIC -O2 -DNDEBUG")

set(LIBRARY_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/lib)
include_directories(${PROJECT_SOURCE_DIR})

file(GLOB NET_SRCS jpmuduo/net/*.cpp)
file(GLOB POLLER_SRCS jpmuduo/net/poller/*.cpp)
file(GLOB INSPECTOR_SRCS jpmuduo/net/inspector/*.cpp)
file(GLOB BASE_SRCS jpmuduo/base/*.cpp)
file(GLOB HTTP_SRCS jpmuduo/net/http/*.cpp)
add_library(jpmuduo SHARED ${NET_SRCS} ${POLLER_SRCS} ${INSPECTOR_SRCS} ${BASE_SRCS} ${HTTP_SRCS})
target_link_libraries(jpmuduo pthread)

add_subdirectory(examples)
