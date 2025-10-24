# QtSetup.cmake - Qt discovery and automoc/rcc/uic

# Prevent double-include
include_guard(GLOBAL)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Widgets Network Script Multimedia MultimediaWidgets TextToSpeech Sql Concurrent REQUIRED)
# Try to resolve Qt bin dir from Qt5_DIR
get_filename_component(Qt5_BIN_DIR "${Qt5_DIR}/../../../bin" ABSOLUTE)

if (UNIX AND NOT APPLE AND EVA_LINUX_STATIC)
    if (NOT TARGET Qt5::QFcitxPlatformInputContextPlugin)
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
    if (NOT TARGET Qt5::QTextToSpeechEngineFlitePlugin)
        if (EVA_TTS_FLITE_PLUGIN_PATH AND EXISTS "${EVA_TTS_FLITE_PLUGIN_PATH}")
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
            message(WARNING "EVA_LINUX_STATIC enabled but Qt5::QTextToSpeechEngineFlitePlugin target not provided. "
                            "Set EVA_TTS_FLITE_PLUGIN_PATH when using a static Qt build with flite.")
        endif()
    endif()
endif()

message(STATUS "Qt5的bin目录  ${Qt5_BIN_DIR}")
message(STATUS "build目录  ${CMAKE_BINARY_DIR}")
message(STATUS "eva输出目录  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
