# QtSetup.cmake - Qt discovery and automoc/rcc/uic

# Prevent double-include
include_guard(GLOBAL)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(_EVA_QT_COMPONENTS Widgets Network Script Multimedia MultimediaWidgets Sql Concurrent)
if (EVA_ENABLE_QT_TTS)
    list(APPEND _EVA_QT_COMPONENTS TextToSpeech)
endif()
find_package(Qt5 COMPONENTS ${_EVA_QT_COMPONENTS} REQUIRED)

function(eva_sanitize_openssl_link_paths)
    if(NOT DEFINED EVA_OPENSSL_FALLBACK_DIR OR EVA_OPENSSL_FALLBACK_DIR STREQUAL "")
        return()
    endif()
    file(TO_CMAKE_PATH "${EVA_OPENSSL_FALLBACK_DIR}" _eva_fallback_root)
    if(NOT EXISTS "${_eva_fallback_root}/lib/libssl.a" OR NOT EXISTS "${_eva_fallback_root}/lib/libcrypto.a")
        return()
    endif()
    function(_eva_sanitize_target_prop _eva_target _eva_prop)
        get_property(_eva_prop_set TARGET ${_eva_target} PROPERTY ${_eva_prop} SET)
        if(NOT _eva_prop_set)
            return()
        endif()
        get_target_property(_eva_val ${_eva_target} ${_eva_prop})
        if(NOT _eva_val)
            return()
        endif()
        set(_eva_fixed "${_eva_val}")
        string(REPLACE "\\" "/" _eva_fixed "${_eva_fixed}")
        string(REGEX REPLACE "/+" "/" _eva_fixed "${_eva_fixed}")
        string(REGEX REPLACE "[A-Za-z]:/openssl-static-3\\.1\\.1" "${_eva_fallback_root}" _eva_fixed "${_eva_fixed}")
        if(NOT _eva_fixed STREQUAL _eva_val)
            set_property(TARGET ${_eva_target} PROPERTY ${_eva_prop} "${_eva_fixed}")
        endif()
    endfunction()

    get_property(_eva_targets GLOBAL PROPERTY TARGETS)
    get_property(_eva_imported_targets GLOBAL PROPERTY IMPORTED_TARGETS)
    list(APPEND _eva_targets ${_eva_imported_targets})
    set(_eva_force_targets
        Qt5::Core
        Qt5::Network
        Qt5::Gui
        Qt5::Widgets
        Qt5::Multimedia
        Qt5::MultimediaWidgets
        Qt5::Sql
        Qt5::Concurrent
        Qt5::TextToSpeech
    )
    list(APPEND _eva_targets ${_eva_force_targets})
    list(REMOVE_DUPLICATES _eva_targets)
    foreach(_eva_target IN LISTS _eva_targets)
        if(NOT TARGET ${_eva_target})
            continue()
        endif()
        foreach(_eva_prop IN ITEMS
            INTERFACE_LINK_LIBRARIES
            INTERFACE_LINK_OPTIONS
            LINK_LIBRARIES
            LINK_OPTIONS
            IMPORTED_LINK_INTERFACE_LIBRARIES)
            _eva_sanitize_target_prop(${_eva_target} ${_eva_prop})
        endforeach()

        get_target_property(_eva_configs ${_eva_target} IMPORTED_CONFIGURATIONS)
        if(_eva_configs)
            foreach(_eva_cfg IN LISTS _eva_configs)
                _eva_sanitize_target_prop(${_eva_target} "IMPORTED_LINK_INTERFACE_LIBRARIES_${_eva_cfg}")
            endforeach()
        endif()
    endforeach()
endfunction()

eva_sanitize_openssl_link_paths()
# Try to resolve Qt bin dir from Qt5_DIR
get_filename_component(Qt5_BIN_DIR "${Qt5_DIR}/../../../bin" ABSOLUTE)
get_filename_component(Qt5_LIB_DIR "${Qt5_DIR}/../.." ABSOLUTE)
set(EVA_LINUX_STATIC_FLITE_FORCE_STUB OFF)

if (UNIX AND NOT APPLE AND EVA_LINUX_STATIC)
    if (NOT TARGET Qt5::QFcitxPlatformInputContextPlugin)
        if (NOT EVA_FCITX_PLUGIN_PATH)
            set(_qt_fcitx_candidate "${Qt5_LIB_DIR}/libfcitxplatforminputcontextplugin.a")
            if (EXISTS "${_qt_fcitx_candidate}")
                # Auto-populate the fcitx plugin path when the user did not provide one.
                set(EVA_FCITX_PLUGIN_PATH "${_qt_fcitx_candidate}"
                    CACHE FILEPATH "Path to libfcitxplatforminputcontextplugin.a for Linux static builds" FORCE)
                message(STATUS "Auto-detected fcitx platform plugin at ${EVA_FCITX_PLUGIN_PATH}")
            endif()
        endif()
        if (EVA_FCITX_PLUGIN_PATH AND EXISTS "${EVA_FCITX_PLUGIN_PATH}")
            add_library(Qt5::QFcitxPlatformInputContextPlugin STATIC IMPORTED)
            set_target_properties(Qt5::QFcitxPlatformInputContextPlugin PROPERTIES
                IMPORTED_GLOBAL TRUE
                IMPORTED_LOCATION "${EVA_FCITX_PLUGIN_PATH}")
            if (EVA_FCITX_PLUGIN_LIBS)
                set(_fcitx_plugin_libs ${EVA_FCITX_PLUGIN_LIBS})
                set_target_properties(Qt5::QFcitxPlatformInputContextPlugin PROPERTIES
                    INTERFACE_LINK_LIBRARIES "${_fcitx_plugin_libs}")
            endif()
            message(STATUS "Using external fcitx platform plugin at ${EVA_FCITX_PLUGIN_PATH}")
        else()
            message(WARNING "EVA_LINUX_STATIC enabled but Qt5::QFcitxPlatformInputContextPlugin not found. "
                            "Provide EVA_FCITX_PLUGIN_PATH to a static libfcitxplatforminputcontextplugin.a.")
        endif()
    endif()
    if (EVA_ENABLE_QT_TTS)
        if (EVA_LINUX_STATIC_SKIP_FLITE)
            message(STATUS "EVA_LINUX_STATIC_SKIP_FLITE=ON: skipping import of QTextToSpeechEngineFlitePlugin")
        elseif (TARGET Qt5::QTextToSpeechEngineFlitePlugin)
            # Plugin available from the Qt toolchain.
        elseif (EVA_TTS_FLITE_PLUGIN_PATH AND EXISTS "${EVA_TTS_FLITE_PLUGIN_PATH}")
            add_library(Qt5::QTextToSpeechEngineFlitePlugin STATIC IMPORTED)
            set_target_properties(Qt5::QTextToSpeechEngineFlitePlugin PROPERTIES
                IMPORTED_GLOBAL TRUE
                IMPORTED_LOCATION "${EVA_TTS_FLITE_PLUGIN_PATH}")
            if (EVA_TTS_FLITE_PLUGIN_LIBS)
                set(_flite_plugin_libs ${EVA_TTS_FLITE_PLUGIN_LIBS})
                set_target_properties(Qt5::QTextToSpeechEngineFlitePlugin PROPERTIES
                    INTERFACE_LINK_LIBRARIES "${_flite_plugin_libs}")
            endif()
            message(STATUS "Using external flite TTS plugin at ${EVA_TTS_FLITE_PLUGIN_PATH}")
        else()
            message(WARNING "Static Qt TextToSpeech build detected but flite plugin binaries are missing. "
                            "Falling back to a stub so the build can finish; voice output via flite will be disabled.")
            add_compile_definitions(EVA_SKIP_FLITE_PLUGIN)
            set(EVA_LINUX_STATIC_FLITE_FORCE_STUB ON)
        endif()
    endif()
endif()

message(STATUS "Qt5的bin目录  ${Qt5_BIN_DIR}")
message(STATUS "build目录  ${CMAKE_BINARY_DIR}")
message(STATUS "eva输出目录  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
