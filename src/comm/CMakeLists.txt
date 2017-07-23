set(comm_src    packet.c)
add_library( comm STATIC ${comm_src})
target_link_libraries(comm)
