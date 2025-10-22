# PostBuild.cmake - post-build deployment and packaging hooks (Windows: bundle Qt)

# Keep this file ASCII only. All paths are quoted.

# This file expects:
#  - EVA_TARGET (main executable target)
#  - Qt5_BIN_DIR (computed in cmake/QtSetup.cmake)

set(_EVA_BUNDLED_SKILLS_DIR "${CMAKE_SOURCE_DIR}/resource/bundled_skills")
if (EXISTS "${_EVA_BUNDLED_SKILLS_DIR}")
    add_custom_command(TARGET ${EVA_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${_EVA_BUNDLED_SKILLS_DIR}"
                "$<TARGET_FILE_DIR:${EVA_TARGET}>/EVA_SKILLS"
        COMMENT "Copy bundled engineer skills into runtime directory")
endif()

if (WIN32)
    # Always ensure Qt SQLite plugin is available for dev runs (BODY_PACK off as well)
    get_filename_component(Qt5_PLUGINS_DIR "${Qt5_BIN_DIR}/../plugins" ABSOLUTE)
    if (EXISTS "${Qt5_PLUGINS_DIR}/sqldrivers/qsqlite.dll")
        add_custom_command(TARGET ${EVA_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${EVA_TARGET}>/sqldrivers"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${Qt5_PLUGINS_DIR}/sqldrivers/qsqlite.dll"
                    "$<TARGET_FILE_DIR:${EVA_TARGET}>/sqldrivers/qsqlite.dll"
            COMMENT "Copy Qt SQLite (qsqlite.dll) driver"
        )
    else()
        message(WARNING "Qt SQLite plugin not found at ${Qt5_PLUGINS_DIR}/sqldrivers/qsqlite.dll")
    endif()
    if (BODY_PACK)
        # Deploy Qt runtime beside eva.exe using windeployqt unless explicitly skipped
        if (NOT BODY_SKIP_WINDEPLOYQT)
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
        else()
            message(STATUS "BODY_PACK enabled: skipping windeployqt (BODY_SKIP_WINDEPLOYQT=ON)")
        endif()

        # Extra runtime for MinGW builds (optional)
        # When BODY_COPY_MINGW_RUNTIME is ON, copy a minimal set of GCC runtime DLLs.
        # Default is OFF if EVA_STATIC=ON.
        if (MINGW AND BODY_COPY_MINGW_RUNTIME)
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

    endif()

elseif(UNIX)
    if (BODY_PACK)
        add_custom_command(TARGET ${EVA_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${EVA_TARGET}>" "${CMAKE_BINARY_DIR}/AppDir/usr/bin/eva"
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/eva.desktop ${CMAKE_BINARY_DIR}/AppDir/usr/share/applications/eva.desktop
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/resource/logo/eva.png ${CMAKE_BINARY_DIR}/AppDir/usr/share/icons/hicolor/64x64/apps/eva.png
        )
        # 执行打包 使用linuxdeploy linuxdeploy-plugin-qt appimagetool打包  生成的.appimage文件在构建目录下
        add_custom_command(TARGET ${EVA_TARGET} POST_BUILD
            COMMAND "${Qt5_BIN_DIR}/linuxdeploy" "--appdir" "${CMAKE_BINARY_DIR}/AppDir"
            COMMAND env QMAKE="${Qt5_BIN_DIR}/qmake" "${Qt5_BIN_DIR}/linuxdeploy-plugin-qt" "--appdir" "${CMAKE_BINARY_DIR}/AppDir"
            COMMAND "${Qt5_BIN_DIR}/appimagetool" "${CMAKE_BINARY_DIR}/AppDir" "--runtime-file" "${Qt5_BIN_DIR}/runtime-appimage" "${eva_OUTPUT_NAME}.appimage"
        )
    endif()
endif()
