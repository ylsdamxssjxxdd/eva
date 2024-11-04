find_package (Threads REQUIRED)

set(TARGET vulkan-shaders-gen)

install(TARGETS ${TARGET} RUNTIME)
target_compile_features(${TARGET} PRIVATE cxx_std_11)
target_link_libraries(vulkan-shaders-gen PUBLIC Threads::Threads)
