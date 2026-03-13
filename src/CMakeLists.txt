aux_source_directory(. SRC_LIST)
add_library(muduoSelf SHARED ${SRC_LIST})

target_link_libraries(muduoSelf pthread)