aux_source_directory(. EXAMPLE_SRC)

add_executable(testserver ${EXAMPLE_SRC})

target_link_libraries(testserver muduoSelf pthread)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)