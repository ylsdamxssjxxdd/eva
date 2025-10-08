# Repository Guidelines

## Project Structure & Module Organization
- Root:  `CMakeLists.txt` builds Qt5 app `eva`; runtime outputs to `build/bin/`. 
-  `src/`: core app - `xbot.*` (LLM runtime/sampling), `widget.*` (Qt UI), `xnet.*` (link mode), `xtool.*` (tools), `xmcp.*` (MCP), `utils/` (widgets, checks, config). 
-  `resource/`: Qt resources (`*.qrc`, images; fonts added on Linux). 
-  `thirdparty/`: `llama.cpp`, `whisper.cpp`, `stable-diffusion.cpp`, `QHotkey` (built as subprojects). Prefer upstream syncs; see `tools/README_cmake.md` before editing. 
-  `tools/`: helper scripts (`build.sh`, `build-docker.bat`) and CMake notes. 
- The project relies on llama. cpp's server as the inference core. Please refer to `thirdparty\llama.cpp\tools\server` for related interfaces
- You have complete authority to manage this project. By good refactoring, the entire project can be modularized, with better maintainability/stability/operational efficiency, and a better user experience.

## Build, Test, and Development Commands
- Configure & build (Release):
 ```bash 
cmake -B build -DBODY_PACK=OFF -DGGML_CUDA=OFF -DGGML_VULKAN=OFF
cmake --build build --config Release -j
 ``` 
- Run locally:  `./build/bin/eva` (Linux) or `build \\bin\\eva.exe ` (Windows). 
- Package (AppImage/EXE): add  `-DBODY_PACK=ON`.

## Coding Style & Naming Conventions
- C++17 + Qt5.15; 4-space indent; braces on same line (K&R); UTF-8 source.
- Prefer Qt types/containers for UI;  `std::vector` OK in compute paths. 
- Files: headers  `.h`, sources `.cpp`, Qt Designer `.ui`; lowercase names (e.g., `xbot.cpp`, `widget.ui`). 
- Methods/vars lowerCamelCase; classes/types PascalCase; macros/constants UPPER_SNAKE_CASE.

## Testing Guidelines
- No project-local unit tests yet; keep changes easy to exercise via UI flows.
- If adding tests, use QtTest under  `tests/` and wire with `add_test()`. 
- Validate on Windows (MSVC) and Linux; document GPU flags used.

## Commit & Pull Request Guidelines
- Never attempt to create branches or commit using Git

## Security & Configuration Tips
- Do not commit models/large binaries; extend  `.gitignore` if needed. 
- Ensure Qt/CUDA/Vulkan SDKs are on PATH; verify  `build/bin/eva(.exe)` runs before pushing. 


# 给后续智能体的编码提示（重要）

为避免中文注释再次变成问号，请严格遵循以下约定：

- 全部源码文件统一使用 UTF-8 编码（推荐无 BOM）。如使用 MSVC/Windows，请确保编译与编辑器均为 UTF-8：
  - VSCode：设置 files.encoding=utf8；必要时关闭自动猜测编码（files.autoGuessEncoding=false）。
  - Qt Creator：工具-选项-文本编辑器-编码 选择 UTF-8。
  - MSVC 项目：如需，可为编译器添加 /utf-8（本仓库已通过 CMake 配置统一为 UTF-8）。
- 请勿使用 GBK/ANSI 等本地编码保存源码，提交前确认中文注释正常显示（不应出现成片问号）。
- 跨平台换行符保持一致（LF 优先）。在 Windows 编辑时，保持与文件现有换行符一致，避免混用。
- 若需在终端查看中文日志，Windows 建议使用支持 UTF-8 的终端并启用 UTF-8 模式。
- 若发现编码异常：
  1) 先检查编辑器的文件编码；
  2) 使用十六进制或常见工具（如 file、iconv）确认实际编码；
  3) 统一转为 UTF-8 后再修改或提交。

说明：本项目源码约定为 UTF-8。此前 src 目录中多处中文注释因编码问题被保存为问号，本次已修复。后续请严格遵守上述约定。
