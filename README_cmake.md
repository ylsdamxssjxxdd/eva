
## camke提示
- 注意cmake构建类型应该设置为 Release
- 全大写的变量是cmake内置的，前面小写的变量是自定义的
- 编译的结果最终在build/Release目录下

## 如果要更换第三方库的话尝试按如下规则修改
### llama.cpp
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

### stable-diffusion.cpp
- 修改cmakelists.txt中set(BUILD_SHARED_LIBS ON)以支持动态链接
- 删除更改stable-diffusion.cpp中的几处LOG_DEBUG以支持mingw
- 目前不适配llama.cpp的ggml库，将自身ggml库改为sd-ggml以适应，权宜之计
- 修改stable-diffusion.cpp/ggml/src/ggml.c中的FILE * ggml_fopen(const char * fname, const char * mode)函数 -> 只要保留return fopen(fname, mode);其余删除

### whisper.cpp
- 删除自己的ggml文件夹
- 删除whisper.cpp中的whisper_init_from_file_with_params_no_state里的#ifdef _MSC_VER部分以支持中文
- 将examples中的common库更名为whisper-common
- examples/main的taget名称改为whisper-cli
- 让所以对ggml库的链接更名为sd-ggml


### libsndfile 读写wav文件
### libsamplerate 重采样wav
- 手动关闭不需要的组件，例如test等