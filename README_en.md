# EVA

A lightweight Agent desktop restraint (unified OpenAI-compatible interface for local/remote)

<img width="1906" height="984" alt="约定框架-DATE Framework" src="https://github.com/user-attachments/assets/20e655a2-cd60-4649-9fb2-21dced3d2664" />

## Quick Start

1. Download EVA

    - https://pan.baidu.com/s/18NOUMjaJIZsV_Z0toOzGBg?pwd=body

    - Including the eva program frontend and EVA_BACKEND backend.

    - EVA_BACKEND structure: `EVA_BACKEND/<architecture>/<system>/<device>/<project>/` (e.g., `EVA_BACKEND/x86_64/win/cuda/llama.cpp/llama-server.exe`). You can also compile the corresponding backend yourself and place it according to this structure; EVA will automatically recognize it during runtime.

2. Download Models

    - https://pan.baidu.com/s/18NOUMjaJIZsV_Z0toOzGBg?pwd=body

    - EVA_MODELS directory:
      - `EVA_MODELS/llm`: Large language models (main), choose one smaller than your device's memory/vRAM size.
      - `EVA_MODELS/multimodal`: Multimodal models (non-essential).
      - `EVA_MODELS/embedding`: Embedding models (non-essential).
      - `EVA_MODELS/speech2text`: Whisper models (non-essential).
      - `EVA_MODELS/text2speech`: tts.cpp models (non-essential).
      - `EVA_MODELS/text2image`: SD/Flux/Qwen-image/Qwen-image-edit/ models (non-essential).

    - You can also visit https://hf-mirror.com to search; EVA supports almost all open-source large language models.

3. Launch

    - On Windows: Double-click eva.exe to open. On Linux: Run eva.appimage.

    - Under Liunx, EVA and backend executable permissions need to be granted. Enter the EVA folder and run 'chmod -R a+x .' .

    - Ensure the EVA program and the backend are in the same directory during runtime.

4. Load!

    - Click the Load button and select a gguf model to load into memory.

5. Send!

    - Enter chat content in the input area and click Send.
    
## Features

### Basic Features

<details>
<summary> Two Modes </summary>

1. Local Mode: After selecting a gguf model, start the local `llama-server` program, which by default opens port 8080 and can also be accessed via a web page.

2. Connection Mode: Fill in `endpoint/key/model` to switch to a remote model, using the OpenAI-compatible interface (`/v1/chat/completions`).

</details>

<details>

<summary> Two States </summary>

1. Conversation State
   
   - Enter chat content in the input area, and the model will respond.
   
   - Mounted tools can be used.
   
   - Press F1 to take a screenshot, press F2 to record audio; screenshots and recordings will be sent to multimodal or Whisper models for corresponding processing.

2. Completion State
   
   - Type any text in the output area, and the model will complete it.

</details>

<details>
<summary> Six Tools </summary>

```txt
    Attach the "tool protocol" to the system prompt to guide the model to initiate calls with <tool_call>JSON</tool_call>;
    After inference, automatically parse tool requests, execute them, and send the results as "tool_response: ..." until there are no new requests.
```

1. Calculator
   
   - The model outputs calculation formulas to the calculator tool, which returns the results.
   
   - Example: Calculate 888*999
   
   - Difficulty level: ⭐

2. Mouse & Keyboard
   
   - The model outputs action sequences to control the user's mouse and keyboard; the model needs to have vision to complete positioning.
   
   - Example: Help me automatically farm in MapleStory
   
   - Difficulty level: ⭐⭐⭐⭐⭐

3. Software Engineer
   
   - Similar to Cline's automated tool execution chain (execute_command/read_file/write_file/replace_in_file/edit_in_file/list_files/search_content/MCP...).
   
   - Example: Help me build an initial CMake Qt project
   
   - Difficulty level: ⭐⭐⭐⭐⭐

4. Knowledge Base
   
   - The model outputs query text to the knowledge base tool, which returns the three most relevant embedded knowledge entries.
   
   - Requirement: First upload text and build in "Proliferation - Knowledge Base" (start the embedding service → store segments one by one via /v1/embeddings).
   
   - Example: What features does EVA have?
   
   - Difficulty level: ⭐⭐⭐

5. Text-to-Image
   
   - The model outputs painting prompts to the text-to-image tool, which returns the generated image.
   
   - Requirement: Users need to configure the path of the text-to-image model in the proliferation window first; supports SD and Flux models.
   
   - Example: Draw a girl
   
   - Difficulty level: ⭐⭐

6. MCP Tool
   
   - Obtain rich external tools through the MCP service.
   
   - Note: After mounting the tool, you need to go to the proliferation window to configure the MCP service.
   
   - Difficulty level: ⭐⭐⭐⭐⭐

</details>

<details>
<summary> Arbitrary Skills </summary>

Skills are pluggable capability bundles introduced into EVA under the covenant framework. The commander can inject scenario-specific workflows, templates, and scripted instructions for the pilot without changing the main program. Refer to the sample skills in the EVA_SIKLLS directory.

- **Requirements**: After mounting the Software Engineer tool, compress the skill folder into a zip file and drag it into the skills area to import. Each skill must provide `SKILL.md`; its YAML frontmatter describes `name`, `description`, `license`, and other metadata, while the body explains operating steps, input/output formats, and caveats. EVA depends on these fields during parsing and will reject the import if any are missing.

</details>

### Enhanced Features

<details>
<summary> Vision </summary>

- Introduction: In Local Mode + Conversation State, you can mount a vision model. Vision models usually have "mmproj" in their names and only match specific models. After successful mounting, users can select images for pre-decoding to serve as the model's context.

- Activation method: Right-click "Mount Vision" in settings and select mmproj; after dragging/dropping, right-clicking to upload, or pressing F1 to take a screenshot, click "Send" for pre-decoding, then proceed with Q&A.

</details>

<details>
<summary> Audition </summary>

- Introduction: Convert the user's voice to text with the whisper.cpp project, or directly input audio to convert to subtitle files.

- Activation method: Right-click the status area to open "Proliferation - Speech-to-Text", select the Whisper model path; return to the main interface, press F2 to start/end recording, and the transcription will be automatically filled in the input box after ending.

</details>

<details>
<summary> Voice </summary>

- Introduction: Convert the text output by the model to voice and play it automatically using Windows' voice function, or configure a tts.cpp model for text-to-speech.

- Activation method: Right-click the status area to open "Proliferation - Text-to-Speech", select a system voice or load a tts.cpp model and start.

</details>

### Auxiliary Features

<details>
<summary> Model Quantization </summary>

- You can right-click the status area to pop up the proliferation window, and quantize unquantized fp32, fp16, bf16 gguf models in the model quantization tab.

</details>

<details>
<summary> Auto-Monitoring </summary>

- In local conversation state, after mounting vision, you can set the monitoring frame rate; then the latest 1-minute screen frames will be automatically attached to the next sending.

</details>

## Source Code Compilation

<details>
<summary> Expand </summary>

1. Configure the environment
   
   - Install a compiler: Use MSVC or MinGW for Windows; use g++ or clang for Linux.
   
   - Install Qt5.15 library: https://download.qt.io/
   
   - Install cmake: https://cmake.org/

2. Clone the source code
   
   ```bash
   git clone https://github.com/ylsdamxssjxxdd/eva.git
   ```

3. Compile
   
   ```bash
   cd eva
   cmake -B build -DBODY_PACK=OFF
   cmake --build build --config Release -j 8
   ```
   
   - BODY_PACK: A flag for packaging. If enabled, all components will be placed in the bin directory on Windows; all components will be packaged into an AppImage file on Linux, but dependencies such as linuxdeploy need to be configured by yourself.

4. Backend preparation
   
    - Obtain compiled inference programs from upstream or third parties.
    - Alternatively, you can obtain all third-party source codes from the Nerv project and compile them yourself: git clone https://github.com/ylsdamxssjxxdd/nerv.git
    - `EVA_BACKEND/x86_64/win/cuda/llama.cpp/llama-server(.exe)`
    - Architecture: `x86_64`, `x86_32`, `arm64`, `arm32`
    - System: `win`, `linux`
    - Device: `cpu`, `cuda`, `vulkan`, `opencl` (custom extensions allowed)
    - Project: e.g., `llama.cpp`, `whisper.cpp`, `stable-diffusion.cpp`
    - During runtime, EVA only enumerates devices and searches for executable files in the same architecture directory of the local machine, and automatically completes the library search path (Windows: PATH; Linux: LD_LIBRARY_PATH).

5. Packaging and distribution (ready to use after decompression)
   
   - Package the executable (build/bin/eva[.exe]), the同级 directory `EVA_BACKEND/`, necessary third-party components and resources, and the optional `EVA_MODELS/` together;
   - Directory example:
     - `EVA_BACKEND/<arch>/<os>/<device>/llama.cpp/llama-server(.exe)`
     - `EVA_BACKEND/<arch>/<os>/<device>/whisper.cpp/whisper-cli(.exe)`
     - `EVA_BACKEND/<arch>/<os>/<device>/llama-tts/llama-tts(.exe)`
     - `EVA_MODELS/{llm,embedding,speech2text,text2speech,text2image}/...`
   - The program will create `EVA_TEMP/` in the same directory on first startup to save configurations, history, and intermediate products.

</details>


## Acknowledgements

- [llama.cpp](https://github.com/ggerganov/llama.cpp)
- [whisper.cpp](https://github.com/ggerganov/whisper.cpp)
- [stable-diffusion.cpp](https://github.com/leejet/stable-diffusion.cpp)
- [TTS.cpp](https://github.com/mmwillet/TTS.cpp)
- [cpp-mcp](https://github.com/hkr04/cpp-mcp)
- [Qt 5.15](https://www.qt.io/)
