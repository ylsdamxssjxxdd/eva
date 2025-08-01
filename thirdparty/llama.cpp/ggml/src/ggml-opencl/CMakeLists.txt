find_package(OpenCL REQUIRED)
find_package(Python3 REQUIRED)

set(TARGET_NAME ggml-opencl)

ggml_add_backend_library(${TARGET_NAME}
                         ggml-opencl.cpp
                         ../../include/ggml-opencl.h)
target_link_libraries(${TARGET_NAME} PRIVATE ${OpenCL_LIBRARIES})
target_include_directories(${TARGET_NAME} PRIVATE ${OpenCL_INCLUDE_DIRS})

if (GGML_OPENCL_PROFILING)
    message(STATUS "OpenCL profiling enabled (increases CPU overhead)")
    add_compile_definitions(GGML_OPENCL_PROFILING)
endif ()

add_compile_definitions(GGML_OPENCL_SOA_Q)
add_compile_definitions(GGML_OPENCL_TARGET_VERSION=${GGML_OPENCL_TARGET_VERSION})

if (GGML_OPENCL_USE_ADRENO_KERNELS)
    message(STATUS "OpenCL will use matmul kernels optimized for Adreno")
    add_compile_definitions(GGML_OPENCL_USE_ADRENO_KERNELS)
endif ()

if (GGML_OPENCL_EMBED_KERNELS)
    add_compile_definitions(GGML_OPENCL_EMBED_KERNELS)

    set(EMBED_KERNEL_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/kernels/embed_kernel.py")
    file(MAKE_DIRECTORY     "${CMAKE_CURRENT_BINARY_DIR}/autogenerated")

    target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/autogenerated")
endif ()

function(ggml_opencl_add_kernel KNAME)
    set(KERN_HDR ${CMAKE_CURRENT_BINARY_DIR}/autogenerated/${KNAME}.cl.h)
    set(KERN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/kernels/${KNAME}.cl)

    if (GGML_OPENCL_EMBED_KERNELS)
        message(STATUS "opencl: embedding kernel ${KNAME}")

        # Python must be accessible from command line
        add_custom_command(
            OUTPUT ${KERN_HDR}
            COMMAND ${Python3_EXECUTABLE} ${EMBED_KERNEL_SCRIPT} ${KERN_SRC} ${KERN_HDR}
            DEPENDS ${KERN_SRC} ${EMBED_KERNEL_SCRIPT}
            COMMENT "Generate ${KERN_HDR}"
        )

        target_sources(${TARGET_NAME} PRIVATE ${KERN_HDR})
    else ()
        message(STATUS "opencl: adding kernel ${KNAME}")
        configure_file(${KERN_SRC} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/${KNAME}.cl COPYONLY)
    endif ()
endfunction()

set(GGML_OPENCL_KERNELS
    add
    argsort
    clamp
    cpy
    cvt
    diag_mask_inf
    div
    gelu
    gemv_noshuffle_general
    gemv_noshuffle
    get_rows
    glu
    group_norm
    im2col_f32
    im2col_f16
    mul_mat_Ab_Bi_8x4
    mul_mv_f16_f16
    mul_mv_f16_f32_1row
    mul_mv_f16_f32_l4
    mul_mv_f16_f32
    mul_mv_f32_f32
    mul_mv_q4_0_f32
    mul_mv_q4_0_f32_v
    mul_mv_q4_0_f32_8x_flat
    mul_mv_q4_0_f32_1d_8x_flat
    mul_mv_q4_0_f32_1d_16x_flat
    mul_mv_q6_k
    mul_mv_id_q4_0_f32_8x_flat
    mul
    norm
    relu
    rms_norm
    rope
    scale
    set_rows
    sigmoid
    silu
    softmax_4_f32
    softmax_4_f16
    softmax_f32
    softmax_f16
    sub
    sum_rows
    transpose
    concat
    tsembd
    upscale
    tanh
    pad
    repeat
    mul_mat_f16_f32
    conv2d
    conv2d_f16_f32
)

foreach (K ${GGML_OPENCL_KERNELS})
    ggml_opencl_add_kernel(${K})
endforeach()
