
## camke提示
- 注意cmake构建类型应该设置为 Release
- 编译的结果最终在build/bin目录下

## 如果要更换第三方库的话尝试按如下规则修改
### llama.cpp
- 修改llama.cpp/ggml/src/ggml.c中的FILE * ggml_fopen(const char * fname, const char * mode)函数 -> 只要保留return fopen(fname, mode);其余删除
- thirdparty\llama.cpp\ggml\src\CMakeLists.txt 
${CMAKE_RUNTIME_OUTPUT_DIRECTORY} 全部替换为 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}

