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

# Install out-of-tree staged backends under bin/<backend>/
# This mirrors DeviceManager::backendsRootDir() expectations.
foreach(B IN LISTS BACKENDS)
    string(TOLOWER "${B}" BLOW)
    set(STAGE_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BLOW})
    install(PROGRAMS
        ${STAGE_DIR}/llama-server${sfx_NAME}
        ${STAGE_DIR}/llama-quantize${sfx_NAME}
        ${STAGE_DIR}/llama-tts${sfx_NAME}
        ${STAGE_DIR}/whisper-cli${sfx_NAME}
        ${STAGE_DIR}/sd${sfx_NAME}
        DESTINATION bin/${BLOW}
    )
endforeach()