# EVA

A smooth llama.cpp launcher + lightweight Agent desktop app (unified OpenAI-compatible interface for local/remote)

<img src="https://github.com/ylsdamxssjxxdd/eva/assets/63994076/a7c5943a-aa4f-4e46-a6c6-284be990fd59" width="300px">

## Features

- Easy to use operating interface 🧮

- Native operational efficiency 🚀

- Intuitive running process 👀

- Comprehensive model support 🐳

- Concise implementation of agent 🤖

- Rich proliferation function 🐣

## Quick start

1. Download EVA

    - https://github.com/ylsdamxssjxxdd/eva/releases

    - download programs with the .exe suffix for Windows or .AppImage suffix for Linux

    ```txt
        The cpu version for the best compatibility; The cuda version uses an NVIDIA graphics card for acceleration and requires cuda to be installed on the computer; The Vulkan version uses any graphics card for acceleration and requires the computer to have a graphics card installed
        
        Under Linux, this command needs to be entered in the terminal to grant EVA permission to run: chmod 777 ***** AppImage 
        It's better to .AppImage is placed in a stable pure English path and only needs to be run once .AppImage will automatically configure desktop shortcuts and start menu
    ```

2. Prepare Models/Backends

    - Optional: place third-party executables under the same-folder `EVA_BACKEND/<arch>/<device>/<project>/` (e.g. `EVA_BACKEND/x86_64/cuda/llama.cpp/llama-server.exe`). EVA will auto-detect the best available device backend at runtime.

    - Models (gguf recommended): keep them under `EVA_MODELS` for painless management:
      - `EVA_MODELS/llm` for LLMs;
      - `EVA_MODELS/embedding` for embeddings;
      - `EVA_MODELS/speech2text` for Whisper;
      - `EVA_MODELS/text2speech` for OuteTTS & WavTokenizer;
      - `EVA_MODELS/text2image` for SD/Flux (default prefers `sd1.5-anything-3-q8_0.gguf`).

    - On the first launch (no config), EVA auto-discovers sensible defaults from `EVA_MODELS/**` and persists them to `EVA_TEMP/eva_config.ini`.

    - https://huggingface.com , eva supports almost all open-source llms

3. Load

    - Click the load button, select a gguf model to load into memory

4. Send！

    - Enter the chat content in the input area and click send

5. Accelerate!

    - Click Settings to adjust GPU offload (ngl). If VRAM is sufficient, push to the maximum; beware >95% VRAM may cause stutters. On the first load EVA will estimate whether full offload is feasible.

    - If running SD simultaneously, make sure to leave enough VRAM for SD


## Function

### Foundation

<details>

<summary> Two mode </summary>

1. Local mode: click Load, pick a gguf model. EVA runs `llama.cpp tools/server` locally and talks over HTTP+SSE.

2. Link mode: right-click Load, fill `endpoint/key/model` and switch to a remote OpenAI-compatible endpoint (`/v1/chat/completions`).

</details>

<details>

<summary> Two state </summary>

1. Chat state

    - The default state, where chat content is entered in the input area and the model responds

    - You can set the system prompt template in the Date panel

    - You can mount tools for the model, but they may affect the model's intelligence

    - You can take a screenshot by pressing f1 and record speech by pressing f2. The screenshot and recording will be sent to the multimodal or whisper for corresponding processing

2. Completion state

    - Typing any text in the output area and the model completing it

 

</details>

<details>

<summary> Six tool </summary>

In local mode and chat state, you can click on the date button to mount the tool

```txt
    EVA injects a tool-calling protocol in the system prompt. The model issues a `<tool_call>{"name":...,"arguments":...}</tool_call>` JSON; EVA executes, then continues with "tool_response: ..." until no more calls.
```

1. calculator

    - Model output the calculation formula to the calculator tool, and the tool will return the calculation result

    - Example: Calculate 888 * 999

    - Difficulty of calling: ⭐

2. controller

    - The model outputs a sequence of actions to control the user's mouse and keyboard, requiring the model to have vision to complete positioning

    - Example: playing games

    - Difficulty of calling: ⭐⭐⭐⭐⭐

3. engineer

    - Automated engineer toolchain like Cline (execute_command/read_file/write_file/edit_file/list_files/search_content/MCP …)

    - Example: help me build an initial project for cmake qt

    - Difficulty of calling: ⭐⭐⭐⭐⭐

4. knowledge

    - The tool computes embeddings and returns Top-N similar chunks.

    - Requirement: you need to upload documents and build a knowledge base in the proliferation window first

    - Example: What are the functions of the EVA?

    - Difficulty of calling: ⭐⭐⭐

    <img src="https://github.com/ylsdamxssjxxdd/eva/assets/63994076/a0b8c4e7-e8dd-4e08-bcb2-2f890d77d632" width="500px">

5. stablediffusion

    - Model output drawing prompt words to the stablediffusion tool, which will return the drawn 

    - Requirement: configure the text2image model path in the Proliferation window (supports SD & Flux).

    - Example: drawing a girl

    - Difficulty of calling: ⭐⭐

    <img src="https://github.com/ylsdamxssjxxdd/eva/assets/63994076/627e5cd2-2361-4112-9df4-41b908fb91c7" width="500px">

6. MCPtools

    - Through MCP services, access a wealth of external tools

    - Explanation: After mounting the tool, you need to go to the proliferation window to configure the MCP service

    - Difficulty of calling：⭐⭐⭐⭐⭐

</details>

### Enhancements

<details>

<summary> Visual </summary>

- Introduction: In Local Mode + Conversation State, you can mount visual models. Visual models typically have "mmproj" in their name and are usually compatible with specific models. Once successfully mounted, users can select an image for pre-decoding, which will serve as the context for the model.

- Activation: In Settings, right-click the "load mmproj" input to choose mmproj; drag image / right-click upload / press F1 to capture; click Send to pre-decode the image, then ask questions.

</details>

<details>

<summary> Auditory </summary>

- Introduction: With the help of the whisper.cpp project, the user's speech can be converted to text.You can also directly input audio and convert it into subtitle files

- Activation: Right-click status area to open Proliferation → speech2text, choose your whisper model. Press F2 to start/stop recording; result is transcribed back to the input area.

</details>

<details>

<summary> Speech </summary>

- Introduction: Using the speech function of the Windows system or outetts model, the llm's output text can be converted to speech and automatically played.

- Activation: Right-click status area → Proliferation → text2speech, choose a voice (system or OuteTTS+WavTokenizer) and start.

</details>

### Auxiliary functions

<details>

<summary> Model quantification </summary>

- You can right-click on the status area to pop up a proliferation window, and quantify the unquantized gguf models of fp32, fp16, and bf16 in the model quantization tab

</details>

<details>

<summary> Automatic monitoring </summary>

- In local chat mode with vision mounted, set a monitor frame rate; EVA will automatically attach recent screen frames (last 60s) to the next send, then clean up expired frames.

</details>

## Build

<details>

<summary> expand </summary>

1. Configure the environment

    - installing the compiler for Windows can be done using MSVC or MingW, while Linux requires g++ or Clang

    - install Qt5.15 https://download.qt.io/

    - install cmake https://cmake.org/

    - nvidia gpu accelerate, install cuda-tooklit https://developer.nvidia.com/cuda-toolkit-archive

    - more gpu accelerate, install VulkanSDK https://vulkan.lunarg.com/sdk/home

2. Clone source code

    ```bash
    git clone https://github.com/ylsdamxssjxxdd/eva.git
    ```

3. Prepare Backends

- Obtain prebuilt binaries and create an `EVA_BACKEND/` folder at the project root (next to `CMakeLists.txt`).
- Follow the central layout: `EVA_BACKEND/<arch>/<device>/<project>/`, for example:
  - `EVA_BACKEND/x86_64/cuda/llama.cpp/llama-server(.exe)`
  - arch: `x86_64`, `x86_32`, `arm64`, `arm32`
  - device: `cpu`, `cuda`, `vulkan`, `opencl` (extend as needed)
  - project: e.g. `llama.cpp`, `whisper.cpp`, `llama-tts`
- At runtime, EVA enumerates devices only under the same-arch folder and discovers executables there; it also prepends the program folder to the dynamic library search path (Windows: PATH; Linux: LD_LIBRARY_PATH; macOS: DYLD_LIBRARY_PATH).

4. Build

    ```bash
    cd eva
    cmake -B build -DBODY_PACK=OFF
    cmake --build build --config Release
    ```

    - BODY_PACK: packaging switch for Linux AppImage, requires your own linuxdeploy setup.

5. Distribute (Unzip-and-run)

    - Bundle the executable (build/bin/eva[.exe]), `EVA_BACKEND/`, required thirdparty/ resources, and optional `EVA_MODELS/` into one package;
    - Example layout:
      - `EVA_BACKEND/<arch>/<device>/llama.cpp/llama-server(.exe)`
      - `EVA_BACKEND/<arch>/<device>/whisper.cpp/whisper-cli(.exe)`
      - `EVA_BACKEND/<arch>/<device>/llama-tts/llama-tts(.exe)`
      - `EVA_MODELS/{llm,embedding,speech2text,text2speech,text2image}/...`
    - On first launch, EVA creates `EVA_TEMP/` next to the app to store config, history and artifacts.

</details>

## Concepts

<details>

<summary> expand </summary>

- model: Composed of a formula and a set of parameters

- token: The number of words, for example, hello token=123, my token=14, his token=3249, different model numbers are different

- vocab: The tokens for all words set during the training of this model are different for different model word lists

- kv cache: The keys and values of the previously calculated model's attention mechanism are equivalent to the model's memory

- decoding: The model calculates a vector table based on the context cache and the incoming new token, and obtains a new context cache

- sampling: Calculate the probability table based on the vector table and select the next word

- predict: (Decoding + Sampling) Loop

- predecode: Decode only without sampling, used to cache context such as system instructions

## Hints: Speed & Memory UI

- After each turn ends, the status line prints `single decode` (generation speed) and `batch decode` (prompt processing speed) in tokens/s; speeds come from server-side `timings` in SSE when available.
- The "memory" progress bar shows context cache usage percentage (tooltip shows used/max tokens). Local mode uses server logs for precise correction; link mode uses a streaming approximation.

## Notes & Limits

- KV reuse: we reuse server-assigned `slot_id` within the same endpoint/session. Persistent `slot-save-path` is not enabled by default.
- Engineer tool safety: file-related tools are restricted to the engineer work directory (EVA_WORK), and all paths are normalized back to that root.

---

- n_ctx_train: The maximum number of tokens that can be decoded during model training

- n_ctx: The maximum number of tokens that the model can accept during decoding set by the you cannot exceed n_ctx_train, which is equivalent to memory capacity

- temperature: During sampling, the vector table will be converted into a probability table based on the temperature value, and the higher the temperature, the greater the randomness

- vecb: The probability distribution of all tokens in the word list during this decoding

- prob: The final selection probability of all tokens in the vocabulary in this sampling

</details>
