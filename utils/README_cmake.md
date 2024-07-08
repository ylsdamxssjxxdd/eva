
## camke提示
- 注意cmake构建类型应该设置为 Release
- 更换编译选项记得清空sd的build目录
- 全大写的变量是cmake内置的，前面小写的变量是自定义的
- mingw编译器用静态链接，msvc编译器使用winrar打包，最终都将得到一个单独的exe在build/Release目录下
- mingw编译时，mingw对应的bin目录需要在环境变量中用于sd.exe编译
- linux编译默认是动态链接的

## 如果要更换第三方库的话尝试按如下规则修改
### llama.cpp 当前版本 b3334
- 修改llama.cpp/ggml/src/ggml.c中的FILE * ggml_fopen(const char * fname, const char * mode)函数 -> 只要保留return fopen(fname, mode);其余删除
- 修改llama.cpp/examples/server/cmakelists.txt -> add_custom_command中xxd.cmake文件路径修改为 "${PROJECT_SOURCE_DIR}/llama.cpp/scripts/xxd.cmake"
- 修改llama.cpp/examples/server/httplib.h的mmap::open函数
```c++
inline bool mmap::open(const char *path) {
  close();

#if defined(_WIN32)
  std::wstring wpath;
  for (size_t i = 0; i < strlen(path); i++) {
    wpath += path[i];
  }

  hFile_ = ::CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                         NULL, OPEN_EXISTING, 0, NULL);

  if (hFile_ == INVALID_HANDLE_VALUE) { return false; }

  LARGE_INTEGER size{};
  if (!::GetFileSizeEx(hFile_, &size)) { return false; }
  size_ = static_cast<size_t>(size.QuadPart);

  hMapping_ =
      ::CreateFileMappingW(hFile_, NULL, PAGE_READONLY, size.HighPart, size.LowPart, NULL);

  if (hMapping_ == NULL) {
    close();
    return false;
  }

  addr_ = ::MapViewOfFile(hMapping_, FILE_MAP_READ, 0, 0, 0);
#else
  fd_ = ::open(path, O_RDONLY);
  if (fd_ == -1) { return false; }

  struct stat sb;
  if (fstat(fd_, &sb) == -1) {
    close();
    return false;
  }
  size_ = static_cast<size_t>(sb.st_size);

  addr_ = ::mmap(NULL, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
#endif

  if (addr_ == nullptr) {
    close();
    return false;
  }

  return true;
}
```

### stable-diffusion.cpp 当前版本 master-9c51d87
- stable-diffusion.cpp/cmakelists.txt中添加
```cmake
# mingw设置编译选项
if(MINGW)
    set(CMAKE_CXX_FLAGS_RELEASE "-static")#对齐静态编译的标志
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -std=c++11 -march=native -Wall -Wextra -ffunction-sections -fdata-sections -fexceptions -mthreads")    
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections -s") #编译优化
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a") # 这里是强制静态链接libgomp.a以支持openmp算法
endif()
```
- 删除更改stable-diffusion.cpp中的几处LOG_DEBUG以支持mingw
- 修改ggml.c中的FILE * ggml_fopen(const char * fname, const char * mode)函数 -> 只要保留return fopen(fname, mode);其余删除

### whisper.cpp 当前版本 1.6.2
- 只需要部分文件即可
