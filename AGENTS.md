# Repository Guidelines

## Project Structure & Module Organization
- Root:  `CMakeLists.txt` builds Qt5 app `eva`; runtime outputs to `build/bin/`. 
-  `src/`: core app - `xbot.*` (LLM runtime/sampling), `widget.*` (Qt UI), `xnet.*` (link mode), `xtool.*` (tools), `xmcp.*` (MCP), `utils/` (widgets, checks, config). 
-  `resource/`: Qt resources (`*.qrc`, images; fonts added on Linux). 
-  `thirdparty/`: `llama.cpp`, `whisper.cpp`, `stable-diffusion.cpp`, `QHotkey` (built as subprojects). Prefer upstream syncs; see `tools/README_cmake.md` before editing. 
-  `tools/`: helper scripts (`build.sh`, `build-docker.bat`) and CMake notes. 
- The project relies on llama. cpp's server as the inference core. Please refer to `thirdparty\llama.cpp\tools\server` for related interfaces

## Build, Test, and Development Commands
- Configure & build (Release):
 ```bash 
cmake -B build -DBODY_PACK=OFF -DGGML_CUDA=OFF -DGGML_VULKAN=OFF
cmake --build build --config Release -j
 ``` 
- Run locally:  `./build/bin/eva` (Linux) or `build \\bin\\eva.exe ` (Windows). 
- Package (AppImage/EXE): add  `-DBODY_PACK=ON`; details in `tools/README_cmake.md`. 

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
