add_executable(viewer viewer.c)
find_package(X11)
find_library(M_LIB m)
target_link_libraries(viewer comm ${OPENGLES3_LIBRARY} ${EGL_LIBRARY} ${X11_LIBRARIES} ${M_LIB})
