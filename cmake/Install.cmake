# Install.cmake - installation layout for make install and AppImage staging

# Prevent double-include
include_guard(GLOBAL)

# Install main GUI executable
install(TARGETS ${EVA_TARGET}
        RUNTIME DESTINATION bin)

# Desktop integration (Linux only)
if (UNIX)
    # .desktop and icon for linuxdeploy / desktop environments
    install(FILES src/utils/eva.desktop
            DESTINATION share/applications)
    install(FILES resource/logo/blue_logo.png
            DESTINATION share/icons/hicolor/64x64/apps)
endif()

# Ship helper conversion scripts from llama.cpp
install(DIRECTORY thirdparty/llama.cpp/gguf-py
        DESTINATION scripts
        USE_SOURCE_PERMISSIONS)
install(FILES thirdparty/llama.cpp/convert_hf_to_gguf.py
        DESTINATION scripts)

# Install out-of-tree staged backends under bin/backend/<backend>/<project>
# This mirrors DeviceManager::backendsRootDir() expectations.
foreach(B IN LISTS BACKENDS)
    string(TOLOWER "${B}" BLOW)
    set(STAGE_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/backend/${BLOW})
    install(PROGRAMS
        ${STAGE_DIR}/llama.cpp/llama-server${sfx_NAME}
        ${STAGE_DIR}/llama.cpp/llama-quantize${sfx_NAME}
        ${STAGE_DIR}/llama.cpp/llama-tts${sfx_NAME}
        DESTINATION bin/backend/${BLOW}/llama.cpp)
    install(PROGRAMS
        ${STAGE_DIR}/whisper.cpp/whisper-cli${sfx_NAME}
        DESTINATION bin/backend/${BLOW}/whisper.cpp)
    install(PROGRAMS
        ${STAGE_DIR}/stable-diffusion.cpp/sd${sfx_NAME}
        DESTINATION bin/backend/${BLOW}/stable-diffusion.cpp)
endforeach()
