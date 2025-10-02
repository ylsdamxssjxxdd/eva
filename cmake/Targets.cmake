# Targets.cmake - define main executable and link

# Prevent double-include
include_guard(GLOBAL)

# Resources
if (WIN32)
    set(resource_FILES resource/res.qrc)
else()
    set(resource_FILES resource/res.qrc resource/font.qrc)
endif()
set(logo_FILES resource/logo/ico.rc)

# Executable
add_executable(
    ${EVA_TARGET}
    ${BODY_PACK_EXE}
    ${logo_FILES} ${resource_FILES} ${extra_INCLUDES}
    src/main.cpp src/widget.cpp src/widget_funcs.cpp src/expend.cpp src/xbot.cpp src/xnet.cpp src/xtool.cpp src/xmcp.cpp
    src/widget.h src/xbot.h src/xtool.h src/expend.h src/xnet.h src/xconfig.h src/xmcp.h src/prompt.h
    src/widget.ui src/expend.ui src/date_dialog.ui src/settings_dialog.ui
    src/utils/csvtablewidget.h
    src/utils/gpuchecker.h src/utils/waterwaveplaintextedit.h src/utils/cpuchecker.h src/utils/customqplaintextedit.h src/utils/doubleqprogressbar.h 
    src/utils/imageinputbox.h src/utils/cutscreendialog.h src/utils/customtabwidget.h src/utils/customswitchbutton.h src/utils/toggleswitch.h src/utils/statusindicator.h
    thirdparty/tinyexpr/tinyexpr.c thirdparty/tinyexpr/tinyexpr.h
)

target_link_libraries(${EVA_TARGET} PRIVATE common llama mtmd ${extra_LIBS} Qt5::Widgets Qt5::Network Qt5::Multimedia Qt5::TextToSpeech mcp QHotkey::QHotkey)
add_dependencies(${EVA_TARGET} llama-server whisper-cli llama-quantize llama-tts sd mtmd ggml)

message(STATUS "生产环境: ${eva_ENVIRONMENT}")
message(STATUS "eva型号：${eva_OUTPUT_NAME}")
