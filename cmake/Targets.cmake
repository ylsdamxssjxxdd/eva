# Targets.cmake - define main executable and link

# Prevent double-include
include_guard(GLOBAL)

# Resources
if (WIN32)
    set(resource_FILES resource/res_core.qrc)
else()
    set(resource_FILES resource/res_core.qrc resource/font.qrc)
endif()
if (BODY_PACK)
    list(APPEND resource_FILES resource/res_docs.qrc)
endif()
set(logo_FILES resource/logo/ico.rc)


add_executable(
    ${EVA_TARGET}
    ${BODY_PACK_EXE}
    ${logo_FILES} ${resource_FILES} ${extra_INCLUDES}
    src/main.cpp src/widget/widget.cpp src/widget/widget_funcs.cpp
    src/widget/widget_link.cpp src/widget/widget_api.cpp src/widget/widget_state.cpp src/widget/widget_date.cpp src/widget/widget_settings.cpp 
    src/widget/widget_slots.cpp src/widget/widget_output.cpp src/widget/widget_anim.cpp
    src/expend/expend_knowledge.cpp src/expend/expend_ui.cpp src/expend/expend_quantize.cpp src/expend/expend_whisper.cpp src/expend/expend_sd.cpp 
    src/expend/expend_eval.cpp 
    src/expend/expend_mcp.cpp src/expend/expend_tts.cpp
    src/expend/expend.cpp src/xnet.cpp src/xtool.cpp src/xmcp.cpp src/xbackend.cpp src/prompt_builder.cpp
    src/utils/history_store.cpp
    src/utils/vectordb.cpp src/utils/vectordb.h
    src/utils/docparser.cpp src/utils/docparser.h
    src/utils/devicemanager.cpp src/utils/devicemanager.h
    src/utils/pathutil.cpp src/utils/pathutil.h src/utils/processrunner.cpp src/utils/processrunner.h src/utils/depresolver.cpp src/utils/depresolver.h
    src/widget/widget.h src/xtool.h src/expend/expend.h src/xnet.h src/xconfig.h src/xmcp.h src/prompt.h src/xbackend.h
    src/widget/widget.ui src/expend/expend.ui src/widget/date_dialog.ui src/widget/settings_dialog.ui
    src/utils/csvtablewidget.h
    src/utils/gpuchecker.h src/utils/cpuchecker.h src/utils/doubleqprogressbar.h 
    src/utils/imageinputbox.h src/utils/cutscreendialog.h src/utils/customtabwidget.h src/utils/customswitchbutton.h src/utils/toggleswitch.h src/utils/statusindicator.h
    thirdparty/tinyexpr/tinyexpr.c thirdparty/tinyexpr/tinyexpr.h
    src/utils/recordbar.cpp src/utils/recordbar.h
    src/utils/neuronlogedit.cpp src/utils/neuronlogedit.h
    src/utils/introanimedit.cpp src/utils/introanimedit.h
)

target_link_libraries(${EVA_TARGET} PRIVATE ${extra_LIBS} Qt5::Widgets Qt5::Network Qt5::Multimedia Qt5::TextToSpeech Qt5::Sql mcp QHotkey::QHotkey)
target_compile_features(${EVA_TARGET} PRIVATE cxx_std_17)
## include build dir for generated config header
target_include_directories(${EVA_TARGET} PRIVATE ${CMAKE_BINARY_DIR}/src/utils ${CMAKE_SOURCE_DIR})

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











