find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)

add_executable(local_server_args_tests
    local_server_args_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/service/backend/xbackend_args.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/pathutil.cpp
)
target_link_libraries(local_server_args_tests PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    eva_doctest
)
target_include_directories(local_server_args_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(local_server_args_tests PRIVATE cxx_std_17)

add_executable(device_manager_tests
    device_manager_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/devicemanager.cpp
)
target_link_libraries(device_manager_tests PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    eva_doctest
)
target_include_directories(device_manager_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(device_manager_tests PRIVATE cxx_std_17)

if (MINGW)
    if (DEFINED EVA_COMPILE_OPTIONS)
        target_compile_options(local_server_args_tests PRIVATE ${EVA_COMPILE_OPTIONS})
        target_compile_options(device_manager_tests PRIVATE ${EVA_COMPILE_OPTIONS})
    endif()
    if (DEFINED EVA_LINK_OPTIONS)
        target_link_options(local_server_args_tests PRIVATE ${EVA_LINK_OPTIONS})
        target_link_options(device_manager_tests PRIVATE ${EVA_LINK_OPTIONS})
    endif()
endif()

add_test(NAME device_manager_tests COMMAND device_manager_tests)
set_tests_properties(device_manager_tests PROPERTIES LABELS unit)
