set(comm_src  packet.c pthreadpool.c)

find_package (Threads)

add_library( comm STATIC ${comm_src})

target_link_libraries(comm pthread)
