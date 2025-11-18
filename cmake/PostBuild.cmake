# PostBuild.cmake - post-build deployment and packaging hooks (Windows: bundle Qt)

# Keep this file ASCII only. All paths are quoted.

# This file expects:
#  - EVA_TARGET (main executable target)
#  - Qt5_BIN_DIR (computed in cmake/QtSetup.cmake)
set(_EVA_BUNDLED_SKILLS_DIR "${CMAKE_SOURCE_DIR}/resource/skills")
if (EXISTS "${_EVA_BUNDLED_SKILLS_DIR}")
    add_custom_command(TARGET ${EVA_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${_EVA_BUNDLED_SKILLS_DIR}"
                "$<TARGET_FILE_DIR:${EVA_TARGET}>/EVA_SKILLS"
        COMMENT "Copy bundled engineer skills into runtime directory")
endif()

if (UNIX)
    # Resolve linker scripts/symlinks to real shared objects so we can grab the SONAME later.
    function(_eva_resolve_library_real_path _out_var _requested_path)
        if (NOT EXISTS "${_requested_path}")
            set(${_out_var} "" PARENT_SCOPE)
            return()
        endif()

        set(_resolved "")
        # GNU ld scripts start with INPUT/GROUP, try to parse the actual targets.
        file(STRINGS "${_requested_path}" _ld_lines LIMIT_COUNT 8)
        foreach(_ld_line IN LISTS _ld_lines)
            string(STRIP "${_ld_line}" _ld_line_stripped)
            if (_ld_line_stripped MATCHES "^(INPUT|GROUP)\\s*\\(([^\\)]+)\\)")
                set(_payload "${CMAKE_MATCH_2}")
                string(REGEX REPLACE "[ \t]+" ";" _entries "${_payload}")
                foreach(_entry IN LISTS _entries)
                    string(STRIP "${_entry}" _candidate)
                    string(REGEX REPLACE "^[\"']" "" _candidate "${_candidate}")
                    string(REGEX REPLACE "[\"']$" "" _candidate "${_candidate}")
                    if (IS_ABSOLUTE "${_candidate}" AND EXISTS "${_candidate}")
                        set(_resolved "${_candidate}")
                        break()
                    endif()
                endforeach()
                if (_resolved)
                    break()
                endif()
            endif()
        endforeach()

        if (NOT _resolved)
            get_filename_component(_realpath "${_requested_path}" REALPATH)
            if (EXISTS "${_realpath}")
                set(_resolved "${_realpath}")
            endif()
        endif()

        set(${_out_var} "${_resolved}" PARENT_SCOPE)
    endfunction()

    # Extract SONAME via objdump so we can recreate versioned links inside AppDir/usr/lib.
    function(_eva_extract_soname _out_var _lib_path)
        set(_soname "")
        if (CMAKE_OBJDUMP AND EXISTS "${CMAKE_OBJDUMP}" AND EXISTS "${_lib_path}")
            execute_process(
                COMMAND "${CMAKE_OBJDUMP}" -p "${_lib_path}"
                OUTPUT_VARIABLE _objdump_text
                RESULT_VARIABLE _objdump_rc
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
            if (_objdump_rc EQUAL 0)
                string(REGEX MATCH "SONAME[ \t]+([^ \t\r\n]+)" _match "${_objdump_text}")
                if (_match)
                    set(_soname "${CMAKE_MATCH_1}")
                endif()
            endif()
        endif()
        set(${_out_var} "${_soname}" PARENT_SCOPE)
    endfunction()
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
        set(_EVA_APPDIR "${CMAKE_BINARY_DIR}/AppDir")
        set(_EVA_APPDIR_BIN "${_EVA_APPDIR}/usr/bin")
        set(_EVA_APPDIR_LIB "${_EVA_APPDIR}/usr/lib")
        set(_EVA_APPDIR_SHARE "${_EVA_APPDIR}/usr/share")
        set(_EVA_APPDIR_APPS "${_EVA_APPDIR_SHARE}/applications")
        set(_EVA_APPDIR_ICONS "${_EVA_APPDIR_SHARE}/icons/hicolor/64x64/apps")

        # Collect extra copy commands for optional executables
        set(_EVA_APPIMAGE_BIN_COPY_COMMANDS "")
        set(_EVA_APPIMAGE_EXTRA_BIN_DESTINATIONS "")
        if (EVA_APPIMAGE_EXTRA_BINS)
            foreach(_extra_bin IN LISTS EVA_APPIMAGE_EXTRA_BINS)
                if (EXISTS "${_extra_bin}")
                    get_filename_component(_extra_bin_name "${_extra_bin}" NAME)
                    set(_extra_bin_dest "${_EVA_APPDIR_BIN}/${_extra_bin_name}")
                    list(APPEND _EVA_APPIMAGE_BIN_COPY_COMMANDS
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                "${_extra_bin}"
                                "${_extra_bin_dest}")
                    list(APPEND _EVA_APPIMAGE_EXTRA_BIN_DESTINATIONS "${_extra_bin_dest}")
                else()
                    message(WARNING "EVA_APPIMAGE_EXTRA_BINS entry '${_extra_bin}' does not exist and will be skipped.")
                endif()
            endforeach()
        endif()

        # Collect extra copy commands for shared libraries (auto + manual)
        set(_EVA_APPIMAGE_LIB_COPY_COMMANDS "")
        set(_EVA_APPIMAGE_EXTRA_LIB_DESTINATIONS "")
        set(_EVA_APPIMAGE_LIB_SOURCE_PATHS "")
        if (EVA_APPIMAGE_BUNDLE_COMMON_LIBS AND EVA_APPIMAGE_COMMON_LIB_NAMES)
            set(_EVA_APPIMAGE_AUTO_LIBS "")
            foreach(_auto_lib_name IN LISTS EVA_APPIMAGE_COMMON_LIB_NAMES)
                string(REGEX REPLACE "[^A-Za-z0-9_]" "_" _auto_lib_var "${_auto_lib_name}")
                find_library(EVA_APPIMAGE_LIB_${_auto_lib_var} NAMES "${_auto_lib_name}")
                if (EVA_APPIMAGE_LIB_${_auto_lib_var})
                    list(APPEND _EVA_APPIMAGE_AUTO_LIBS "${EVA_APPIMAGE_LIB_${_auto_lib_var}}")
                    message(STATUS "AppImage: found ${_auto_lib_name} at ${EVA_APPIMAGE_LIB_${_auto_lib_var}}")
                else()
                    message(STATUS "AppImage: missing ${_auto_lib_name}, skip auto bundle")
                endif()
            endforeach()
            if (_EVA_APPIMAGE_AUTO_LIBS)
                list(APPEND _EVA_APPIMAGE_LIB_SOURCE_PATHS ${_EVA_APPIMAGE_AUTO_LIBS})
                message(STATUS "EVA AppImage auto bundling libs: ${_EVA_APPIMAGE_AUTO_LIBS}")
            else()
                message(STATUS "EVA AppImage auto bundling libs: none found for current toolchain.")
            endif()
        endif()
        if (EVA_APPIMAGE_EXTRA_LIBS)
            list(APPEND _EVA_APPIMAGE_LIB_SOURCE_PATHS ${EVA_APPIMAGE_EXTRA_LIBS})
        endif()
        if (_EVA_APPIMAGE_LIB_SOURCE_PATHS)
            list(REMOVE_DUPLICATES _EVA_APPIMAGE_LIB_SOURCE_PATHS)
            message(STATUS "EVA AppImage bundling libs (auto+extra): ${_EVA_APPIMAGE_LIB_SOURCE_PATHS}")
            foreach(_extra_lib IN LISTS _EVA_APPIMAGE_LIB_SOURCE_PATHS)
                if (EXISTS "${_extra_lib}")
                    get_filename_component(_requested_lib_name "${_extra_lib}" NAME)
                    set(_resolved_lib "${_extra_lib}")
                    if (UNIX)
                        _eva_resolve_library_real_path(_resolved_lib "${_extra_lib}")
                    endif()
                    if (NOT _resolved_lib)
                        message(WARNING "AppImage library source '${_extra_lib}' cannot be resolved and will be skipped.")
                        continue()
                    endif()
                    get_filename_component(_resolved_lib_name "${_resolved_lib}" NAME)
                    set(_resolved_lib_dest "${_EVA_APPDIR_LIB}/${_resolved_lib_name}")
                    list(APPEND _EVA_APPIMAGE_LIB_COPY_COMMANDS
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                "${_resolved_lib}"
                                "${_resolved_lib_dest}")
                    list(APPEND _EVA_APPIMAGE_EXTRA_LIB_DESTINATIONS "${_resolved_lib_dest}")
                    if (UNIX)
                        _eva_extract_soname(_resolved_soname "${_resolved_lib}")
                        if (_resolved_soname)
                            set(_soname_dest "${_EVA_APPDIR_LIB}/${_resolved_soname}")
                            if (NOT _soname_dest STREQUAL _resolved_lib_dest)
                                list(APPEND _EVA_APPIMAGE_LIB_COPY_COMMANDS
                                    COMMAND ${CMAKE_COMMAND} -E remove -f "${_soname_dest}"
                                    COMMAND ${CMAKE_COMMAND} -E create_symlink
                                            "${_resolved_lib_name}"
                                            "${_soname_dest}")
                            endif()
                        endif()
                        if (_requested_lib_name
                            AND NOT _requested_lib_name STREQUAL _resolved_lib_name
                            AND (NOT _resolved_soname OR NOT _requested_lib_name STREQUAL _resolved_soname))
                            set(_requested_dest "${_EVA_APPDIR_LIB}/${_requested_lib_name}")
                            list(APPEND _EVA_APPIMAGE_LIB_COPY_COMMANDS
                                COMMAND ${CMAKE_COMMAND} -E remove -f "${_requested_dest}"
                                COMMAND ${CMAKE_COMMAND} -E create_symlink
                                        "${_resolved_lib_name}"
                                        "${_requested_dest}")
                        endif()
                    endif()
                else()
                    message(WARNING "AppImage library source '${_extra_lib}' does not exist and will be skipped.")
                endif()
            endforeach()
        endif()

        # Compose linuxdeploy arguments (allow bundling of extra shared libs/executables)
        set(_EVA_LINUXDEPLOY_ARGS "--appdir" "${_EVA_APPDIR}" "--executable" "${_EVA_APPDIR_BIN}/eva")
        if (_EVA_APPIMAGE_EXTRA_LIB_DESTINATIONS)
            foreach(_extra_lib_dest IN LISTS _EVA_APPIMAGE_EXTRA_LIB_DESTINATIONS)
                list(APPEND _EVA_LINUXDEPLOY_ARGS "--library" "${_extra_lib_dest}")
            endforeach()
        endif()
        if (_EVA_APPIMAGE_EXTRA_BIN_DESTINATIONS)
            foreach(_extra_bin_dest IN LISTS _EVA_APPIMAGE_EXTRA_BIN_DESTINATIONS)
                list(APPEND _EVA_LINUXDEPLOY_ARGS "--executable" "${_extra_bin_dest}")
            endforeach()
        endif()

        set(_EVA_LINUXDEPLOY_COMMANDS
            COMMAND "${Qt5_BIN_DIR}/linuxdeploy" ${_EVA_LINUXDEPLOY_ARGS})
        if (EVA_LINUX_STATIC)
            list(APPEND _EVA_LINUXDEPLOY_COMMANDS
                COMMAND "${Qt5_BIN_DIR}/appimagetool" "${_EVA_APPDIR}" "--runtime-file" "${Qt5_BIN_DIR}/runtime-appimage" "${eva_OUTPUT_NAME}.appimage")
        else()
            list(APPEND _EVA_LINUXDEPLOY_COMMANDS
                COMMAND env QMAKE="${Qt5_BIN_DIR}/qmake" "${Qt5_BIN_DIR}/linuxdeploy-plugin-qt" "--appdir" "${_EVA_APPDIR}"
                COMMAND "${Qt5_BIN_DIR}/appimagetool" "${_EVA_APPDIR}" "--runtime-file" "${Qt5_BIN_DIR}/runtime-appimage" "${eva_OUTPUT_NAME}.appimage")
        endif()

        add_custom_command(TARGET ${EVA_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_EVA_APPDIR_BIN}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_EVA_APPDIR_LIB}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_EVA_APPDIR_APPS}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_EVA_APPDIR_ICONS}"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${EVA_TARGET}>" "${_EVA_APPDIR_BIN}/eva"
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/eva.desktop "${_EVA_APPDIR_APPS}/eva.desktop"
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/resource/logo/eva.png "${_EVA_APPDIR_ICONS}/eva.png"
            ${_EVA_APPIMAGE_BIN_COPY_COMMANDS}
            ${_EVA_APPIMAGE_LIB_COPY_COMMANDS}
            ${_EVA_LINUXDEPLOY_COMMANDS}
        )
    endif()
endif()
