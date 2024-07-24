
set(TARGET vulkan-shaders-gen)
add_executable(${TARGET} vulkan-shaders-gen.cpp)
target_link_libraries(vulkan-shaders-gen ${CMAKE_THREAD_LIBS_INIT})
install(TARGETS ${TARGET} RUNTIME)
target_compile_features(${TARGET} PRIVATE cxx_std_11)
