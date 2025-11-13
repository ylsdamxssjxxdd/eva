# Targets.cmake - define main executable and link

# Prevent double-include
include_guard(GLOBAL)

# Resources
set(resource_FILES resource/res_core.qrc resource/font.qrc)
list(APPEND resource_FILES resource/res_docs.qrc)
set(logo_FILES resource/logo/ico.rc)


add_executable(
    ${EVA_TARGET}
    ${BODY_PACK_EXE}
    ${logo_FILES} ${resource_FILES} ${extra_INCLUDES}
    src/main.cpp src/widget/widget.cpp src/widget/widget_funcs.cpp
    src/widget/widget_link.cpp src/widget/widget_api.cpp src/widget/widget_state.cpp src/widget/widget_date.cpp src/widget/widget_settings.cpp 
    src/widget/widget_slots.cpp src/widget/widget_output.cpp src/widget/widget_anim.cpp src/widget/terminal_pane.cpp
    src/widget/toolcall_test_dialog.cpp src/widget/toolcall_test_dialog.h
    src/expend/expend_knowledge.cpp src/expend/expend_ui.cpp src/expend/expend_quantize.cpp src/expend/expend_whisper.cpp src/expend/expend_sd.cpp 
    src/expend/expend_eval.cpp 
    src/expend/expend_mcp.cpp src/expend/expend_tts.cpp
    src/expend/sd_params_dialog.cpp src/expend/sd_params_dialog.h
    src/expend/expend.cpp src/xnet.cpp src/net/localproxy.cpp src/xtool.cpp src/xmcp.cpp src/xbackend.cpp src/prompt_builder.cpp
    src/utils/history_store.cpp
    src/utils/vectordb.cpp src/utils/vectordb.h
    src/utils/docparser.cpp src/utils/docparser.h
    src/utils/devicemanager.cpp src/utils/devicemanager.h
    src/utils/pathutil.cpp src/utils/pathutil.h src/utils/processrunner.cpp src/utils/processrunner.h src/utils/depresolver.cpp src/utils/depresolver.h
    src/utils/startuplogger.cpp src/utils/startuplogger.h
    src/utils/singleinstance.cpp src/utils/singleinstance.h
    src/widget/widget.h src/widget/terminal_pane.h src/xtool.h src/expend/expend.h src/xnet.h src/xconfig.h src/xmcp.h src/prompt.h src/xbackend.h
    src/widget/widget.ui src/expend/expend.ui src/widget/date_dialog.ui src/widget/settings_dialog.ui
    src/utils/gpuchecker.h src/utils/cpuchecker.h src/utils/doubleqprogressbar.h 
    src/utils/imageinputbox.h src/utils/cutscreendialog.h src/utils/customtabwidget.h src/utils/customswitchbutton.h src/utils/toggleswitch.h src/utils/statusindicator.h
    thirdparty/tinyexpr/tinyexpr.c thirdparty/tinyexpr/tinyexpr.h
    src/utils/recordbar.cpp src/utils/recordbar.h
    src/utils/neuronlogedit.cpp src/utils/neuronlogedit.h
    src/utils/evallogedit.cpp src/utils/evallogedit.h
    src/utils/introanimedit.cpp src/utils/introanimedit.h
    src/utils/textspacing.cpp src/utils/textspacing.h
    src/utils/minibarchart.cpp  src/utils/minibarchart.h src/utils/flowprogressbar.h
    src/utils/zip_extractor.cpp src/utils/zip_extractor.h
    src/utils/static_plugin_stubs.cpp
    src/skill/skill_manager.cpp src/skill/skill_manager.h
    src/widget/skill_drop_area.cpp src/widget/skill_drop_area.h
    src/net/localproxy.h
    thirdparty/miniz/miniz.c thirdparty/miniz/miniz_zip.c thirdparty/miniz/miniz_tinfl.c thirdparty/miniz/miniz_tdef.c
)
## Executable name
# Linux: keep binary name as plain "eva" for runtime/AppDir consistency
# Other platforms: keep the versioned/output-friendly name
if (UNIX)
    set_target_properties(${EVA_TARGET} PROPERTIES OUTPUT_NAME "eva")
else()
    set_target_properties(${EVA_TARGET} PROPERTIES OUTPUT_NAME "${eva_OUTPUT_NAME}")
endif()
target_link_libraries(${EVA_TARGET} PRIVATE ${extra_LIBS} Qt5::Widgets Qt5::Network Qt5::Multimedia Qt5::MultimediaWidgets Qt5::Sql Qt5::Concurrent qtmcp QHotkey::QHotkey)
target_compile_features(${EVA_TARGET} PRIVATE cxx_std_17)
## include build dir for generated config header
target_include_directories(${EVA_TARGET} PRIVATE
    ${CMAKE_BINARY_DIR}/src/utils
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/thirdparty/miniz
    ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann)

if (EVA_ENABLE_QT_TTS)
    target_link_libraries(${EVA_TARGET} PRIVATE Qt5::TextToSpeech)
endif()

if (UNIX AND NOT APPLE AND EVA_LINUX_STATIC)
    if (TARGET Qt5::QFcitxPlatformInputContextPlugin)
        target_link_libraries(${EVA_TARGET} PRIVATE Qt5::QFcitxPlatformInputContextPlugin)
    elseif (EVA_FCITX_PLUGIN_PATH)
        target_link_libraries(${EVA_TARGET} PRIVATE "${EVA_FCITX_PLUGIN_PATH}")
        if (EVA_FCITX_PLUGIN_LIBS)
            target_link_libraries(${EVA_TARGET} PRIVATE ${EVA_FCITX_PLUGIN_LIBS})
        endif()
    else()
        message(WARNING "Static build requested but Qt5::QFcitxPlatformInputContextPlugin target not available. Fcitx IM may be missing.")
    endif()
    if (EVA_ENABLE_QT_TTS)
        if (NOT EVA_LINUX_STATIC_SKIP_FLITE AND NOT EVA_LINUX_STATIC_FLITE_FORCE_STUB AND TARGET Qt5::QTextToSpeechEngineFlitePlugin)
            target_link_libraries(${EVA_TARGET} PRIVATE Qt5::QTextToSpeechEngineFlitePlugin)
        elseif (NOT EVA_LINUX_STATIC_SKIP_FLITE AND NOT EVA_LINUX_STATIC_FLITE_FORCE_STUB AND EVA_TTS_FLITE_PLUGIN_PATH)
            target_link_libraries(${EVA_TARGET} PRIVATE "${EVA_TTS_FLITE_PLUGIN_PATH}")
            if (EVA_TTS_FLITE_PLUGIN_LIBS)
                target_link_libraries(${EVA_TARGET} PRIVATE ${EVA_TTS_FLITE_PLUGIN_LIBS})
            endif()
        elseif (EVA_LINUX_STATIC_SKIP_FLITE)
            message(STATUS "EVA_LINUX_STATIC_SKIP_FLITE=ON: not linking Flite text-to-speech plugin")
        elseif (EVA_LINUX_STATIC_FLITE_FORCE_STUB)
            message(STATUS "Static Linux build: flite text-to-speech plugin unavailable; using stub implementation instead.")
        else()
            message(WARNING "Static TextToSpeech build detected but Flite plugin target is unavailable; voice features may fail.")
        endif()
    endif()
endif()

# Apply MinGW-specific compile/link options collected in ProjectOptions.cmake
if (MINGW)
    if (DEFINED EVA_COMPILE_OPTIONS)
        target_compile_options(${EVA_TARGET} PRIVATE ${EVA_COMPILE_OPTIONS})
        message(STATUS "EVA_COMPILE_OPTIONS: ${EVA_COMPILE_OPTIONS}")
    endif()
    if (DEFINED EVA_LINK_OPTIONS)
        target_link_options(${EVA_TARGET} PRIVATE ${EVA_LINK_OPTIONS})
        message(STATUS "EVA_LINK_OPTIONS: ${EVA_LINK_OPTIONS}")
    endif()
endif()

# Ensure prebuilt backends are copied before building/running ${EVA_TARGET}
# This makes `--target eva` also execute the backend copy step.
if (TARGET backends)
    add_dependencies(${EVA_TARGET} backends)
endif()

message(STATUS "eva型号: ${eva_OUTPUT_NAME}")














