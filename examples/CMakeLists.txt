add_executable(testclient testclient.cpp)
add_executable(testserver testserver.cpp)
add_executable(benchserver benchserver.cpp)
add_executable(benchmark benchmark.cpp)
add_executable(server_test server_test.cpp)

target_link_libraries(testserver jpmuduo pthread)
target_link_libraries(benchserver jpmuduo pthread)
target_link_libraries(server_test jpmuduo pthread)

add_executable(client_test client_test.cpp)
target_link_libraries(client_test jpmuduo pthread)

add_executable(buffer_framing_test buffer_framing_test.cpp)
target_link_libraries(buffer_framing_test jpmuduo pthread)

add_executable(http_test http_test.cpp)
target_link_libraries(http_test jpmuduo pthread)

add_executable(pingpong pingpong.cpp)

add_executable(echoserver echoserver.cpp)
target_link_libraries(echoserver jpmuduo pthread)

add_executable(conn_stress conn_stress.cpp)
target_link_libraries(conn_stress jpmuduo pthread)

add_executable(echoclient echoclient.cpp)
target_link_libraries(echoclient jpmuduo pthread)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
add_executable(inspector_demo inspector_demo.cpp)
target_link_libraries(inspector_demo jpmuduo pthread)

add_executable(udp_test udp_test.cpp)
target_link_libraries(udp_test jpmuduo pthread)
