#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "clblast" for configuration ""
set_property(TARGET clblast APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(clblast PROPERTIES
  IMPORTED_IMPLIB_NOCONFIG "${_IMPORT_PREFIX}/lib/libclblast.dll.a"
  IMPORTED_LINK_INTERFACE_LIBRARIES_NOCONFIG "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.8/lib/x64/OpenCL.lib"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/bin/libclblast.dll"
  )

list(APPEND _cmake_import_check_targets clblast )
list(APPEND _cmake_import_check_files_for_clblast "${_IMPORT_PREFIX}/lib/libclblast.dll.a" "${_IMPORT_PREFIX}/bin/libclblast.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
