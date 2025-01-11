set(TARGET whisper-cli)
add_executable(${TARGET} main.cpp)


target_link_libraries(${TARGET} PRIVATE whisper-common whisper ${FFMPEG_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
