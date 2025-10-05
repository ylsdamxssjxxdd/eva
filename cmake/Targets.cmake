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

# Executable
add_executable(
    ${EVA_TARGET}
    ${BODY_PACK_EXE}
    ${logo_FILES} ${resource_FILES} ${extra_INCLUDES}
    src/main.cpp src/widget/widget.cpp src/widget/widget_funcs.cpp
    src/widget/widget_link.cpp src/widget/widget_api.cpp src/widget/widget_state.cpp src/widget/widget_date.cpp src/widget/widget_settings.cpp 
    src/widget/widget_slots.cpp src/widget/widget_output.cpp src/widget/widget_load.cpp src/widget/widget_anim.cpp
    src/expend/expend_knowledge.cpp src/expend/expend_ui.cpp src/expend/expend_quantize.cpp src/expend/expend_whisper.cpp src/expend/expend_sd.cpp 
    src/expend/expend_mcp.cpp src/expend/expend_convert.cpp src/expend/expend_brain.cpp src/expend/expend_tts.cpp
    src/expend/expend.cpp src/xnet.cpp src/xtool.cpp src/xmcp.cpp src/xbackend.cpp src/prompt_builder.cpp
    src/utils/history_store.cpp
    src/utils/devicemanager.cpp src/utils/devicemanager.h
    src/widget/widget.h src/xtool.h src/expend/expend.h src/xnet.h src/xconfig.h src/xmcp.h src/prompt.h src/xbackend.h
    src/widget/widget.ui src/expend/expend.ui src/widget/date_dialog.ui src/widget/settings_dialog.ui
    src/utils/csvtablewidget.h
    src/utils/gpuchecker.h src/utils/waterwaveplaintextedit.h src/utils/cpuchecker.h src/utils/customqplaintextedit.h src/utils/doubleqprogressbar.h 
    src/utils/imageinputbox.h src/utils/cutscreendialog.h src/utils/customtabwidget.h src/utils/customswitchbutton.h src/utils/toggleswitch.h src/utils/statusindicator.h
    thirdparty/tinyexpr/tinyexpr.c thirdparty/tinyexpr/tinyexpr.h
)

target_link_libraries(${EVA_TARGET} PRIVATE ${extra_LIBS} Qt5::Widgets Qt5::Network Qt5::Multimedia Qt5::TextToSpeech mcp QHotkey::QHotkey)
target_compile_features(${EVA_TARGET} PRIVATE cxx_std_17)
## include build dir for generated config header
target_include_directories(${EVA_TARGET} PRIVATE ${CMAKE_BINARY_DIR}/src/utils)

message(STATUS "eva型号: ${eva_OUTPUT_NAME}")

# Ensure third-party command-line tools are built and staged alongside eva
add_dependencies(${EVA_TARGET} backends)




