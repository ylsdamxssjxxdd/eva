
## camke提示
- 注意cmake构建类型应该设置为 Release
- 全大写的变量是cmake内置的，前面小写的变量是自定义的
- 编译的结果最终在build/Release目录下

## 如果要更换第三方库的话尝试按如下规则修改
### llama.cpp
- 修改llama.cpp/ggml/src/ggml.c中的FILE * ggml_fopen(const char * fname, const char * mode)函数 -> 只要保留return fopen(fname, mode);其余删除
- thirdparty\llama.cpp\ggml\src\ggml-vulkan\CMakeLists.txt 
${CMAKE_RUNTIME_OUTPUT_DIRECTORY} 全部替换为 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}
- 修改llama.cpp/examples/server/cmakelists.txt -> add_custom_command中xxd.cmake文件路径修改为 "${PROJECT_SOURCE_DIR}/thirdparty/llama.cpp/scripts/xxd.cmake"
- 禁用掉thirdparty\llama.cpp\common\CMakeLists.txt 里的add_custom_command相关 但是确保手动加上build-info.cpp文件
- 注释掉llama-bench.cpp main中的setlocale(LC_CTYPE, ".UTF-8"); 以支持中文
- thirdparty\llama.cpp\ggml\src\ggml-cpu\ggml-cpu-impl.h 中添加这段代码以支持飞腾cpu的编译

```txt
// 替代vld1q_s8_x4
inline static int8x16x4_t vld1q_s8_x4(const int8_t *ptr) {
    int8x16x4_t ret;
    ret.val[0] = vld1q_s8(ptr);
    ret.val[1] = vld1q_s8(ptr + 16);
    ret.val[2] = vld1q_s8(ptr + 32);
    ret.val[3] = vld1q_s8(ptr + 48);
    return ret;
}

// 替代 vld1q_u8_x4 的实现
inline static uint8x16x4_t vld1q_u8_x4(const uint8_t *ptr) {
    uint8x16x4_t ret;
    ret.val[0] = vld1q_u8(ptr);      // 加载第 0-15 字节
    ret.val[1] = vld1q_u8(ptr + 16); // 加载第 16-31 字节
    ret.val[2] = vld1q_u8(ptr + 32); // 加载第 32-47 字节
    ret.val[3] = vld1q_u8(ptr + 48); // 加载第 48-63 字节
    return ret;
}
```

### stable-diffusion.cpp
- 删除自己的ggml文件夹
- 修改cmakelists.txt中set(BUILD_SHARED_LIBS ON)以支持动态链接
- 删除更改stable-diffusion.cpp中的几处LOG_DEBUG以支持mingw
- 如果要依赖本身的ggml，则改名为sd-前缀的所有ggml库，尤其是在链接时target_link_libraries要链接sd-前缀的库
- 以及ggml-blas

### whisper.cpp
- 删除自己的ggml文件夹
- 删除whisper.cpp中的whisper_init_from_file_with_params_no_state里的#ifdef _MSC_VER部分以支持中文
- 将examples中的common库更名为whisper-common
- examples/main的taget名称改为whisper-cli
- src里控制ggml库的链接

### libsndfile 读写wav文件
### libsamplerate 重采样wav
- 手动关闭不需要的组件，例如test等