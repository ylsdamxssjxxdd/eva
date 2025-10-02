# PostBuild.cmake - OS-specific deployment and packaging

if (MSVC)
    add_custom_command(TARGET  POST_BUILD
        COMMAND "/windeployqt.exe" "--release" "--no-translations" "/"
        COMMENT "custom Windeployqt ..."
    )
    add_custom_command(TARGET  POST_BUILD
        COMMAND  -E copy_directory "/thirdparty/llama.cpp/gguf-py" "/scripts/gguf-py"
        COMMAND  -E copy "/thirdparty/llama.cpp/convert_hf_to_gguf.py" "/scripts/convert_hf_to_gguf.py"
        COMMENT "copy scripts"
    )
    if (GGML_CUDA)
        add_custom_command(TARGET  POST_BUILD
            COMMAND  -E copy
                "/cublas64_.dll"
                "/cublas64_.dll"
            COMMAND  -E copy
                "/cudart64_.dll"
                "/cudart64_.dll"
            COMMAND  -E copy
                "/cublasLt64_.dll"
                "/cublasLt64_.dll"
            COMMENT "Copying CUDA libraries to output directory"
        )
    endif()
    if (BODY_PACK)
        add_custom_command(TARGET  POST_BUILD
            COMMAND  -E remove "/vc_redist.x64.exe"
            COMMAND  -E remove_directory "/EVA_TEMP"
            COMMAND  -E copy "/" "/"
            COMMENT "custom Removing ..."
        )
    endif()
elseif (MINGW)
    get_filename_component(COMPILER_BIN_DIR "" DIRECTORY)
    message(STATUS "Compiler bin directory: ")
    add_custom_command(TARGET  POST_BUILD
        COMMAND "/windeployqt.exe" "--release" "--no-translations" "/"
        COMMAND  -E copy "/libgomp-1.dll" "/libgomp-1.dll"
        COMMAND  -E copy "/" "/"
        COMMENT "custom Windeployqt ..."
    )
    add_custom_command(TARGET  POST_BUILD
        COMMAND  -E copy_directory "/thirdparty/llama.cpp/gguf-py" "/scripts/gguf-py"
        COMMAND  -E copy "/thirdparty/llama.cpp/convert_hf_to_gguf.py" "/scripts/convert_hf_to_gguf.py"
        COMMENT "copy scripts"
    )
elseif (UNIX)
    add_custom_command(TARGET  POST_BUILD
        COMMAND  -E copy_directory "/thirdparty/llama.cpp/gguf-py" "/scripts/gguf-py"
        COMMAND  -E copy "/thirdparty/llama.cpp/convert_hf_to_gguf.py" "/scripts/convert_hf_to_gguf.py"
        COMMENT "copy scripts"
    )
    if (BODY_PACK)
        add_custom_command(TARGET  POST_BUILD
            COMMAND  -E remove "/eva-*.AppImage"
            COMMAND  -E copy "/eva" "/AppDir/usr/bin/eva"
            COMMAND  -E copy "/llama-server" "/AppDir/usr/bin/llama-server"
            COMMAND  -E copy "/llama-quantize" "/AppDir/usr/bin/llama-quantize"
            COMMAND  -E copy "/llama-tts" "/AppDir/usr/bin/llama-tts"
            COMMAND  -E copy "/bin/libstable-diffusion.so" "/AppDir/usr/lib/libstable-diffusion.so"
            COMMAND  -E copy "/sd" "/AppDir/usr/bin/sd"
            COMMAND  -E copy "/thirdparty/whisper.cpp/src/libwhisper.so.1" "/AppDir/usr/lib/libwhisper.so.1"
            COMMAND  -E copy "/thirdparty/whisper.cpp/src/libwhisper.so.1.7.2" "/AppDir/usr/lib/libwhisper.so.1.7.2"
            COMMAND  -E copy "/whisper-cli" "/AppDir/usr/bin/whisper-cli"
            COMMAND  -E copy "/libsd-ggml.so" "/AppDir/usr/lib/libsd-ggml.so"
            COMMAND  -E copy "/libsd-ggml-cpu.so" "/AppDir/usr/lib/libsd-ggml-cpu.so"
            COMMAND  -E copy "/libsd-ggml-base.so" "/AppDir/usr/lib/libsd-ggml-base.so"
            COMMAND  -E chdir /src/utils  -E copy /src/utils/eva.desktop /AppDir/usr/share/applications/eva.desktop
            COMMAND  -E chdir /resource  -E copy /resource/logo/blue_logo.png /AppDir/usr/share/icons/hicolor/64x64/apps/blue_logo.png
        )
        add_custom_command(TARGET  POST_BUILD
            COMMAND  -E copy_directory "/thirdparty/llama.cpp/gguf-py" "/AppDir/usr/scripts/gguf-py"
            COMMAND  -E copy "/thirdparty/llama.cpp/convert_hf_to_gguf.py" "/AppDir/usr/scripts/convert_hf_to_gguf.py"
            COMMENT "copy scripts"
        )
        add_custom_command(TARGET  POST_BUILD
            COMMAND "/linuxdeploy" "--appdir" "/AppDir"
            COMMAND env QMAKE="/qmake" "/linuxdeploy-plugin-qt" "--appdir" "/AppDir"
            COMMAND "/appimagetool" "/AppDir" "--runtime-file" "/runtime-appimage" ".AppImage"
        )
        if (GGML_CUDA)
            add_custom_command(TARGET  POST_BUILD
                COMMAND  -E copy "/libsd-ggml-cuda.so" "/AppDir/usr/lib/libsd-ggml-cuda.so"
            )
        elseif (GGML_VULKAN)
            add_custom_command(TARGET  POST_BUILD
                COMMAND  -E copy "/libsd-ggml-vulkan.so" "/AppDir/usr/lib/libsd-ggml-vulkan.so"
            )
        endif()
    endif()
endif()
