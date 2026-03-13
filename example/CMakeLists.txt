add_executable(testclient testclient.cpp)
add_executable(testserver testserver.cpp)

target_link_libraries(testserver muduoSelf pthread)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)