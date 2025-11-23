add_library(libxls STATIC
    src/endian.c
    src/locale.c
    src/ole.c
    src/xls.c
    src/xlstool.c)

target_include_directories(libxls
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(libxls PRIVATE HAVE_ICONV=0)
