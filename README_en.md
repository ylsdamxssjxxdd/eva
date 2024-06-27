# eva
Intuitive large model application (based on llama.cpp & qt5)

Video Introduction https://www.bilibili.com/video/BV15r421h728/?share_source=copy_web&vd_source=569c126f2f63df7930affe9a2267a8f8

<img src="https://github.com/ylsdamxssjxxdd/eva/assets/63994076/a7c5943a-aa4f-4e46-a6c6-284be990fd59" width="300px">



## feature
- Intuitive 
    - Clearly show the process of the llm predicting the next word
- Compatibility
    - Support Windows
    - Support Linux
- Multifunctional (all available but not very effective)
    - local model,  online model, API services, WEB services, agent, multimodal
    - knowledge base QA, code interpreter, software control, text2img, voice2text 
    - model quantize, model evaluation
    
## quick start
1. Download eva
- https://github.com/ylsdamxssjxxdd/eva/releases
- download programs with the .exe suffix for Windows or .AppImage suffix for Linux
2. Download gguf model
- https://huggingface.co , eva supports almost all open-source llms
3. Load！
- Click the load button, select a gguf model to load into memory
4. Send！
- Enter the chat content in the input area and click send

## function
1. Chat mode
- The default mode, where chat content is entered in the input area and the model responds
- You can set prompt templatee in date button
- You can mount tools for the model, but they may affect the model's intelligence
- You can upload a CSV format question bank for testing
- You can take a screenshot by pressing f1 and record voice by pressing f2. The screenshot and recording will be sent to the multimodal or whisper for corresponding processing
2. Completion mode
- Typing any text in the output area and the model completing it
3. Service mode
- eva becomes an open API port service and can also chat on web pages
4. Link status
- eva can connect to the chat endpoints of other API services and run without the need to load the model
5. Knowledge base
- Users can upload documents, which are embedded and processed to become the knowledge base of the model. And then you can mount knowledge tool, it can be called by the model

<img src="https://github.com/ylsdamxssjxxdd/eva/assets/63994076/a0b8c4e7-e8dd-4e08-bcb2-2f890d77d632" width="500px">

6. text2img
- Stable-diffusion.cpp can be used to draw images, which can be called by the model after mount

<img src="https://github.com/ylsdamxssjxxdd/eva/assets/63994076/627e5cd2-2361-4112-9df4-41b908fb91c7" width="500px">

7. Debug mode
- The prediction of the next word is manually controlled by the user, which can obtain more information about the model's operation

8. ...

## build
1. Configure the environment
    - install compiler msvc or mingw
    - install Qt5 https://download.qt.io/
    - install cmake https://cmake.org/
    - if we want to accelerate, install cuda-tooklit https://developer.nvidia.com/cuda-toolkit-archive
    - if we want to accelerate, install VulkanSDK https://vulkan.lunarg.com/sdk/home
2. Clone source code
```bash
git clone https://github.com/ylsdamxssjxxdd/eva.git
```
3. Build
```bash
cd eva
cmake -B build -DBODY_PACK=OFF -DLLAMA_CUDA=OFF -DLLAMA_VULKAN=OFF -DBODY_32BIT=OFF 
cmake --build build --config Release
```
-EVA-PACK: Flag indicating whether packaging is required. If enabled, all components will be packaged as a self extracting program in Windows, and all components will be packaged as an AppImage file in Linux
-LLAMA-CUDA: Flag indicating whether cuda acceleration needs to be enabled
-LLAMA_VULKAN: Flag indicating whether vulkan acceleration needs to be enabled
-EVA_12BIT: Flag for compiling to 32-bit program

## guideline
- Load process
    - [ui] -> User clicks on load -> Select path -> Send setting parameters -> [bot] -> Processing parameters -> Send overload signal -> [ui] -> Pre load -> Loading interface status -> Send loading signal -> [bot] -> Start loading -> Send loading animation signal -> After loading reset -> Send loading completion signal -> [ui] -> Accelerate loading animation -> Loading animation end -> Rolling animation start -> Animation end -> Force unlocking -> Trigger sending -> Send pre decoding (only decoding but not sampling output) instruction -> [bot] -> Pre decoding system instruction -> Send decoding completion signal -> [ui] -> Normal interface status -> END
- Send process
    -[ui] -> User clicks send -> Mode/tag/content analysis -> Conversation mode -> Inference interface state -> Send input parameters -> Send inference signal -> [bot] -> Preprocess user input -> Streaming loop output -> Loop termination -> Send inference end signal -> [ui] -> Normal interface state -> END
- Date process
    - [ui] -> User click on agreement -> Display last configuration -> Click confirm -> Record user configuration -> Send agreement parameters -> [bot] -> Record user configuration -> Send agreement reset signal -> [ui] -> Trigger interface reset -> Send reset signal -> [bot] -> Initialize required components for model operation -> Send reset completion signal -> [ui] -> Pre decode if system instructions change -> END
- Set process
    - [ui] -> User clicks on settings -> Display last configuration -> Click confirm -> Record user configuration -> Send setting parameters -> [bot] -> Record user configuration -> Analyze configuration changes -> END/Send overload signal/Send setting reset signal -> [ui] -> Pre load/trigger interface reset -> END
- Predecoding image process
    - [ui] -> User uploads image/presses F1 screenshot -> Trigger send -> Inference interface state -> Send pre decoded image command -> [bot] -> Pre decoded image -> Occupy 1024 tokens -> Send decoding completion signal -> [ui] -> Normal interface state -> END
- Recording to text process
    - 【 ui 】 -> User presses f2 for the first time -> Need to specify the Whisper model path -> Send expend interface display signal -> 【 expend 】 -> Pop up sound reproduction interface -> Select path -> Send Whisper model path -> [ui] -> User presses f2 again -> Recording interface status -> Start recording -> User presses f2 again -> End recording -> Save WAV file to local -> Send WAV file path -> 【 expend 】 -> Call Whisperexe for decoding -> After decoding is completed, save txt result to local -> Send text result -> 【 ui 】 -> Normal interface status -> Display to input area -> END
- Tool call process
    - [ui] -> User click to send -> Mode/tag/content analysis -> Dialogue mode situation -> Inference interface status -> Send input parameters -> Send inference signal -> [bot] -> Preprocess user input -> Streaming loop output -> Loop termination -> Send inference end signal -> [ui] -> Extract JSON field from the current output of the model -> Send JSON field -> Send tool inference signal -> [tool] -> Execute corresponding function based on JSON field -> Return result after execution -> [ui] -> Use the returned result as the sending content and add observation prefix -> Trigger sending -> ·· -> No reasonable JSON field -> Normal interface state -> END
- Building a knowledge base process
    - 【expend】 -> Users enter the knowledge base tab -> Users click to select models -> Select embedded models -> Start server. exe -> Start complete -> Automatically write the v1/embeddings endpoint of the server into the address bar -> Users click to upload and select a txt text -> Text segmentation -> Users can modify the content of the text segment to be embedded as needed -> Users click to embed the text segment -> Send each text segment to the endpoint address and receive the calculated word vector -> Display embedded text segments in the table -> Send embedded text segment data -> 【tool】 -> END
- Knowledge base Q&A process
    - [ui] -> Tool invocation process -> JSON field contains knowledge keyword -> Send JSON field -> Send tool inference signal -> [tool] -> Execute knowledge function -> Send query field to embedded endpoint -> [server] -> Return calculated word vector -> [tool] -> Calculate cosine similarity between query segment word vector and each embedded text segment word vector -> Return the three most similar text segments -> [ui] -> Use the returned result as the sending content and add observation prefix -> Trigger sending -> ··· -> No reasonable JSON field -> Normal interface state -> END
- Link process
    - [ui] -> User right-click load -> Configure IP and endpoints -> Click confirm -> Lock interface -> Record configuration -> Connection test -> Test passed -> Unlock interface -> END
    - The other processes in the linked state are similar to the above, replacing [bot] with [net]
- Debug process
    - [ui] -> The user can pull up the status area to pop up a debug button -> the user can open the debug button -> click send -> enter the debugging state ->send process, only decode and sample once -> click Next -> send process, only decode and sample once -> ··· -> exit the debugging state when a stop flag is detected/the maximum output length is reached/manual stop is reached -> END

## concepts
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
- n_ctx: The maximum number of tokens that the model can accept during decoding set by the user cannot exceed n_ctx_train, which is equivalent to memory capacity
- temperature: During sampling, the vector table will be converted into a probability table based on the temperature value, and the higher the temperature, the greater the randomness
- vecb: The probability distribution of all tokens in the word list during this decoding
- prob: The final selection probability of all tokens in the vocabulary in this sampling

## to do
- ~~Adapt to Linux (completed)~~
- Adapt to more CPU/GPU
- ~~English version (completed)~~

## bugs
- There is a memory leak in model inference, located in the stream function of xbot.cpp, to be fixed
- In link mode, it is not possible to send continuously without intervals. It is alleviated by triggering after a timed 100ms. The QNetworkAccess Manager located in xnet.cpp cannot be released in a timely manner and needs to be fixed
-Multimodal model output abnormality, needs to be aligned with llava.cpp, to be fixed
---
- Truncate once after reaching the maximum context length and then reach it again. Decoding will fail and can be alleviated by temporarily placing empty memory. The llama_decode located in xbot.cpp returns 1 (unable to find the kv slot), which has not been fixed (in fact, after truncation, the number of tokens sent and the reserved part still exceed the maximum context length, and needs to be truncated again)
- Some UTF-8 characters have parsing issues and have been fixed (incomplete UTF-8 characters in model output need to be manually concatenated into one)
- Memory leakage during model switching, fixed (not using MMP when using CUDA)
- The version compiled by Mingw cannot recognize the Chinese path during loading, and is located in the fp=std:: fopen (fname, mode); of llama.cpp;, Fixed (using QTextCodec:: codecForName ("GB2312") to transcode characters)
- CSV files cannot be parsed correctly when there are special symbols, located in the readCsvFile function of utils.cpp, fixed (using an improved parsing method that relies on a simple state machine to track whether text segments are inside quotation marks and correctly handle line breaks within fields)