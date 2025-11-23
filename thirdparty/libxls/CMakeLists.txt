add_library(libxls STATIC
    src/endian.c
    src/locale.c
    src/ole.c
    src/xls.c
    src/xlstool.c)

target_include_directories(libxls
    PUBLIC
        /include
    PRIVATE
        )

target_compile_definitions(libxls PRIVATE HAVE_ICONV=0)
