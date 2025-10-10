# EVA

A smooth llama. cpp launcher

<img src="https://github.com/ylsdamxssjxxdd/eva/assets/63994076/a7c5943a-aa4f-4e46-a6c6-284be990fd59" width="300px">

## Features

- Easy to use operating interface 🧮

- Native operational efficiency 🚀

- Intuitive running process 👀

- Comprehensive model support 🐳

- Concise implementation of agent 🤖

- Rich proliferation function 🐣

## Quick start

1. Download eva

    - https://github.com/ylsdamxssjxxdd/eva/releases

    - download programs with the .exe suffix for Windows or .AppImage suffix for Linux

    ```txt
        The cpu version for the best compatibility; The cuda version uses an NVIDIA graphics card for acceleration and requires cuda to be installed on the computer; The Vulkan version uses any graphics card for acceleration and requires the computer to have a graphics card installed
        
        Under Linux, this command needs to be entered in the terminal to grant EVA permission to run: chmod 777 ***** AppImage 
        It's better to .AppImage is placed in a stable pure English path and only needs to be run once .AppImage will automatically configure desktop shortcuts and start menu
    ```

2. Download gguf model

    - https://huggingface.com , eva supports almost all open-source llms

3. Load！

    - Click the load button, select a gguf model to load into memory

4. Send！

    - Enter the chat content in the input area and click send

5. Accelerate!

    - Click Settings to set gpu offload. Recommended setting to maximum, but if the VRAM occupies more than 95%, it will be too laggy

    - If running SD simultaneously, make sure to leave enough VRAM for SD


## Function

### Foundation function

<details>

<summary> Two mode </summary>

1. Local mode: you left clicks the load button to interact by loading the local model

2. Link mode: you right clicks the load button and inputs the API endpoint of a certain model service for interaction (Currently supports openai type compatible interfaces)

</details>

<details>

<summary> Two state </summary>

1. Chat state

    - The default state, where chat content is entered in the input area and the model responds

    - You can set prompt templatee in date button

    - You can mount tools for the model, but they may affect the model's intelligence

    - You can take a screenshot by pressing f1 and record speech by pressing f2. The screenshot and recording will be sent to the multimodal or whisper for corresponding processing

2. Completion state

    - Typing any text in the output area and the model completing it

 

</details>

<details>

<summary> Six tool </summary>

In local mode and chat state, you can click on the date button to mount the tool

```txt
    The principle is to add an additional instruction in the system instruction to guide the model to call the corresponding tool
    After each model prediction is completed, eva will automatically detects whether it contains the XML field of the calling tool. If it does, the corresponding tool is called. After the tool is executed, the result is sent to the model for further prediction
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

    - An automated tool execution chain similar to Cline

    - Example: help me build an initial project for cmake qt

    - Difficulty of calling: ⭐⭐⭐⭐⭐

4. knowledge

    - Model output query text to the knowledge tool, which will return the three most relevant embedded knowledge items

    - Requirement: you need to upload documents and build a knowledge base in the proliferation window first

    - Example: What are the functions of the EVA?

    - Difficulty of calling: ⭐⭐⭐

    <img src="https://github.com/ylsdamxssjxxdd/eva/assets/63994076/a0b8c4e7-e8dd-4e08-bcb2-2f890d77d632" width="500px">

5. stablediffusion

    - Model output drawing prompt words to the stablediffusion tool, which will return the drawn 

    - Requirement: you need to first configure the model path of the text2image in the proliferation window

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

- Activation: Right-click on the "load mmproj" input box in the settings and select the mmproj model. You can pre-decode an image by dragging it into the input box, right-clicking the input box to click, or pressing F1 to take a screenshot. Then, click the send button to pre-decode the image, and after decoding, you can proceed with the Q&A.

</details>

<details>

<summary> Auditory </summary>

- Introduction: With the help of the whisper.cpp project, the user's speech can be converted to text.You can also directly input audio and convert it into subtitle files

- Activation: Right-click the status area to open the expansion window, select the speech2text tab, and choose the path where the whisper model is located. Return to the main interface, press the F2 shortcut to start recording, press F2 again to end the recording, and it will automatically convert to text and fill into the input area.

</details>

<details>

<summary> Speech </summary>

- Introduction: Using the speech function of the Windows system or outetts model, the llm's output text can be converted to speech and automatically played.

- Activation: Right-click the status area to open the expansion window, select the text2speech tab, and enable a sound source.

</details>

### Auxiliary functions

<details>

<summary> Model quantification </summary>

- You can right-click on the status area to pop up a proliferation window, and quantify the unquantized gguf models of fp32, fp16, and bf16 in the model quantization tab

</details>

<details>

<summary> Automatic monitoring </summary>

- In local mode, after mounting the vision, the monitoring frame rate can be set, and the model will automatically monitor the screen at this frequency

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

- Obtain prebuilt binaries and create a `EVA_BACKEND/` folder at the project root (next to `CMakeLists.txt`).
- Place required executables under `EVA_BACKEND/<device>/` (any subfolder depth; EVA searches recursively):
  - Local LLM: `llama-server` (with its dependent DLLs/SOs in the same folder)
  - Speech2Text: `whisper-cli`
  - Text2Image: `sd`
- The `<device>` name is arbitrary (e.g., `cpu`, `cuda`, `vulkan`, `opencl`, `mygpu`). The Settings dialog lists all first-level folders under `EVA_BACKEND/`.
- During build, CMake copies `EVA_BACKEND/` to `build/bin/EVA_BACKEND/`. At runtime, EVA discovers executables recursively and prepends their folder to the library search path (PATH / LD_LIBRARY_PATH).

4. Build

    ```bash
    cd eva
    cmake -B build -DBODY_PACK=OFF
    cmake --build build --config Release
    ```

    - BODY_PACK: Flag indicating whether packaging is required. If enabled, all components will be place in the bin directory in Windows; and all components will be packaged as an AppImage file in Linux. Note that tools such as linuxdeploy need to be configured by oneself

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

---

- n_ctx_train: The maximum number of tokens that can be decoded during model training

- n_ctx: The maximum number of tokens that the model can accept during decoding set by the you cannot exceed n_ctx_train, which is equivalent to memory capacity

- temperature: During sampling, the vector table will be converted into a probability table based on the temperature value, and the higher the temperature, the greater the randomness

- vecb: The probability distribution of all tokens in the word list during this decoding

- prob: The final selection probability of all tokens in the vocabulary in this sampling

</details>
