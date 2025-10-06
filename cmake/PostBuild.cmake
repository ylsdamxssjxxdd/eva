# PostBuild.cmake - post-build deployment and packaging hooks (Windows: bundle Qt)

# Keep this file ASCII only. All paths are quoted.

# This file expects:
#  - EVA_TARGET (main executable target)
#  - Qt5_BIN_DIR (computed in cmake/QtSetup.cmake)

if (WIN32)
    if (BODY_PACK)
        # Deploy Qt runtime beside eva.exe using windeployqt
        if (EXISTS "${Qt5_BIN_DIR}/windeployqt.exe")
            add_custom_command(TARGET ${EVA_TARGET} POST_BUILD
                COMMAND "${Qt5_BIN_DIR}/windeployqt.exe"
                        --release
                        --no-translations
                        --compiler-runtime
                        --dir "$<TARGET_FILE_DIR:${EVA_TARGET}>"
                        "$<TARGET_FILE:${EVA_TARGET}>"
                COMMENT "windeployqt: bundling Qt runtime into bin"
            )
        else()
            message(WARNING "windeployqt.exe not found under Qt5_BIN_DIR=${Qt5_BIN_DIR}")
        endif()

        # Ship helper conversion scripts next to the exe
        add_custom_command(TARGET ${EVA_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${EVA_TARGET}>/scripts"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_SOURCE_DIR}/thirdparty/llama.cpp/gguf-py"
                    "$<TARGET_FILE_DIR:${EVA_TARGET}>/scripts/gguf-py"
            COMMAND ${CMAKE_COMMAND} -E copy
                    "${CMAKE_SOURCE_DIR}/thirdparty/llama.cpp/convert_hf_to_gguf.py"
                    "$<TARGET_FILE_DIR:${EVA_TARGET}>/scripts/convert_hf_to_gguf.py"
            COMMENT "Copy helper scripts"
        )

        # Extra runtime for MinGW builds
        if (MINGW)
            get_filename_component(COMPILER_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
            add_custom_command(TARGET ${EVA_TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${COMPILER_BIN_DIR}/libgcc_s_seh-1.dll"
                        "$<TARGET_FILE_DIR:${EVA_TARGET}>/libgcc_s_seh-1.dll"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${COMPILER_BIN_DIR}/libstdc++-6.dll"
                        "$<TARGET_FILE_DIR:${EVA_TARGET}>/libstdc++-6.dll"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${COMPILER_BIN_DIR}/libwinpthread-1.dll"
                        "$<TARGET_FILE_DIR:${EVA_TARGET}>/libwinpthread-1.dll"
                COMMENT "Copy MinGW runtime DLLs"
            )
        endif()

        # CUDA DLLs if CUDA backend is enabled (expected names for CUDA 11/12)
        if (GGML_CUDA AND DEFINED CUDAToolkit_BIN_DIR AND DEFINED CUDA_VERSION_MAJOR)
            # add_custom_command(TARGET ${EVA_TARGET} POST_BUILD
            #     COMMAND ${CMAKE_COMMAND} -E copy_if_different
            #             "${CUDAToolkit_BIN_DIR}/cudart64_${CUDA_VERSION_MAJOR}.dll"
            #             "$<TARGET_FILE_DIR:${EVA_TARGET}>/cudart64_${CUDA_VERSION_MAJOR}.dll"
            #     COMMAND ${CMAKE_COMMAND} -E copy_if_different
            #             "${CUDAToolkit_BIN_DIR}/cublas64_${CUDA_VERSION_MAJOR}.dll"
            #             "$<TARGET_FILE_DIR:${EVA_TARGET}>/cublas64_${CUDA_VERSION_MAJOR}.dll"
            #     COMMAND ${CMAKE_COMMAND} -E copy_if_different
            #             "${CUDAToolkit_BIN_DIR}/cublasLt64_${CUDA_VERSION_MAJOR}.dll"
            #             "$<TARGET_FILE_DIR:${EVA_TARGET}>/cublasLt64_${CUDA_VERSION_MAJOR}.dll"
            #     COMMENT "Copy CUDA runtime DLLs"
            # )
        endif()
    endif()

elseif(UNIX)
    if (BODY_PACK)
        add_custom_command(TARGET ${EVA_TARGET} POST_BUILD
            COMMAND bash "${CMAKE_SOURCE_DIR}/tools/package-appimage.sh" "${CMAKE_BINARY_DIR}" "${CMAKE_BINARY_DIR}/AppDir" "$<CONFIG>"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            COMMENT "Packaging AppImage (BODY_PACK=ON)"
        )
    else()
        # Copy helper scripts (when not packaging)
        add_custom_command(TARGET ${EVA_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_SOURCE_DIR}/thirdparty/llama.cpp/gguf-py"
                    "$<TARGET_FILE_DIR:${EVA_TARGET}>/../scripts/gguf-py"
            COMMAND ${CMAKE_COMMAND} -E copy
                    "${CMAKE_SOURCE_DIR}/thirdparty/llama.cpp/convert_hf_to_gguf.py"
                    "$<TARGET_FILE_DIR:${EVA_TARGET}>/../scripts/convert_hf_to_gguf.py"
            COMMENT "Copy helper scripts"
        )
    endif()
endif()