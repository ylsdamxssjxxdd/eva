cmake_minimum_required(VERSION 3.12)

option(EVA_ENABLE_UNIT_TESTS "Build the eva unit test suite" ON)
option(EVA_ENABLE_FUNCTIONAL_TESTS "Build the eva functional test suite" OFF)

if(EVA_ENABLE_UNIT_TESTS)
    add_subdirectory(unit)
endif()

if(EVA_ENABLE_FUNCTIONAL_TESTS)
    add_subdirectory(functional)
endif()
