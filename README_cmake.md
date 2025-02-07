
## camke提示
- 注意cmake构建类型应该设置为 Release
- 全大写的变量是cmake内置的，前面小写的变量是自定义的
- 编译的结果最终在build/Release目录下

## 如果要更换第三方库的话尝试按如下规则修改
### llama.cpp
- 修改llama.cpp/ggml/src/ggml.c中的FILE * ggml_fopen(const char * fname, const char * mode)函数 -> 只要保留return fopen(fname, mode);其余删除
- 修改llama.cpp/examples/server/cmakelists.txt -> add_custom_command中xxd.cmake文件路径修改为 "${PROJECT_SOURCE_DIR}/thirdparty/llama.cpp/scripts/xxd.cmake"
- 搜索 llama.cpp/examples/server/httplib.h 的 #if _WIN32_WINNT >= _WIN32_WINNT_WIN8 注释掉它们
- 禁用掉thirdparty\llama.cpp\common\CMakeLists.txt 里的add_custom_command相关 但是确保存在build-info.cpp文件
- thirdparty\llama.cpp\ggml\src\ggml-vulkan\CMakeLists.txt 
${CMAKE_RUNTIME_OUTPUT_DIRECTORY} 全部替换为 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}

注意#11626 pr后server.exe不再支持win7 原因是index.html.gz的变化，使用b4604的暂时可以

### stable-diffusion.cpp
- 删除自己的ggml文件夹
- 修改cmakelists.txt中set(BUILD_SHARED_LIBS ON)以支持动态链接
- 删除更改stable-diffusion.cpp中的几处LOG_DEBUG以支持mingw
- 如果要依赖本身的ggml，则改名为sd-前缀的所有ggml库，尤其是在链接时target_link_libraries要链接sd-前缀的库

### whisper.cpp
- 删除自己的ggml文件夹
- 删除whisper.cpp中的whisper_init_from_file_with_params_no_state里的#ifdef _MSC_VER部分以支持中文
- 将examples中的common库更名为whisper-common
- examples/main的taget名称改为whisper-cli
- src里控制ggml库的链接

### libsndfile 读写wav文件
### libsamplerate 重采样wav
- 手动关闭不需要的组件，例如test等