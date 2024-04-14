# eva
Intuitive large model application (based on llama.cpp & qt5)

Video Introduction https://www.bilibili.com/video/BV15r421h728/?share_source=copy_web&vd_source=569c126f2f63df7930affe9a2267a8f8

<img src="https://github.com/ylsdamxssjxxdd/eva/assets/63994076/a7c5943a-aa4f-4e46-a6c6-284be990fd59" width="300px">



## feature
- Light
    - eva has no other dependent components, just an exe program
- Compatibility
    - Minimum support for 32-bit Windows 7
    - May be can support linux, macos, android
- Intuitive 
    - Clearly show the process of the llm predicting the next word
- Multifunctional
    - Local model interaction, agent, multimodal, online model interaction, external API services, knowledge base QA, model quantize, text2img, voice2text (all available but not very effective)
    
## quick start
1. Download eva
- https://pan.baidu.com/s/18NOUMjaJIZsV_Z0toOzGBg?pwd=body
- or this Release https://github.com/ylsdamxssjxxdd/eva/releases
2. Download gguf model
- https://pan.baidu.com/s/18NOUMjaJIZsV_Z0toOzGBg?pwd=body
- or https://huggingface.co
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

## build
1. Configure the environment
- 64bit version (all option options are turned off)
    - My toolchain is mingw81_64+qt5.15.10 
- 32bit version (BODY_32BIT option turned on, others turned off)
    - My toolchain is mingw81_12+qt5.15.10 
- Opencl version (LLAMA_CLBLAST option turned on, others turned off)
    - My toolchain is mingw81_64+clblast +qt5.15.10 
    - Download opencl sdk and add its include directory to the environment variable https://github.com/KhronosGroup/OpenCL-SDK
    - Download clblast, turn off the build_shared_lib option, compile it statically and add it to the environment variable https://github.com/CNugteren/CLBlast 
- CUDA version (LLAMA_CUBLAST option turned on, others turned off)
    - My toolchain is msvc2022+qt5.15.10+cuda12
    - Install cuda booklit https://developer.nvidia.com/cuda-toolkit-archive This is also necessary for the runtime of the cuda version of the machine body
    - Install cudnn https://developer.nvidia.com/cudnn
- Vulkan version (LLAMA_VULKAN option turned on, others turned off)
    - My toolchain is mingw81_64+VulkanSDK+qt5.15.10
    - Download VulkanSDK https://vulkan.lunarg.com/sdk/home

2. Clone source code
```bash
git clone https://github.com/ylsdamxssjxxdd/eva.git
```
3. Modify the corresponding configuration according to the instructions in the CMakeLists.txt file
4. Build

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

## concepts
- eva: composed of a restraint device (action plan) and a body (llama. cpp), with the large model being the driver and the user being the commander
- model: composed of a formula (neural network structure) and a set of parameters (connection weights), performing decoding operations (using input text to predict the next word)
- token: The number of words, for example, hello token=123, my token=14, his token=3249, and different model numbers are different
- vocab: The tokens for all words set during model training are different for different models. The higher the proportion of Chinese in the vocabulary, the stronger the Chinese language ability
- ctx: includes a set of parameters and context cache that control the decoding of the model, and the memory occupation is related to the model vocabulary size, context length, batch size, and model size
- KV cache: that is, historical decoding information, equivalent to the memory of the model
- n_ctx_train: The maximum number of tokens that can be sent for decoding during model training
- n_ctx: The maximum number of tokens that the model can accept during decoding set by the user, which cannot exceed n_ctx_train
- vecb: The results obtained by decoding the context cache and incoming tokens by the model
- temperature: During sampling, the vector table will be converted into a probability table based on the temperature value, and the higher the temperature, the greater the randomness
- prob: The selection probability of all tokens in the model vocabulary, used to predict the next token
- lora: Mounts a simple model in the original model, which can change the output style of the model. It does not support CUDA acceleration

## behavior
- Predecoding: Decoding the system instructions agreed upon by the user in advance, and executing them when the user modifies the system instructions/reaches the maximum context
- Reset: Delete cache other than system instructions and clear output area; If predicting, terminate and do not take any other action
- eva loading: Load the model structure and model weights into memory, equivalent to the driver entering the machine eva for synchronization
- eva overload: reloading the model, modifying decoding parameters/switching from web mode/selecting a new model will be executed
- eva expend: Service mode activated, providing web services and API services to the outside world
- eva link: The machine is connected to external API services and can run without loading
- eva overload: If the context cache reaches the set context length, the first half of the cache is discarded
- eva confusion: malfunction in the Xbot decoding of the model control part of the organism
- eva broken: loading failure caused by model structure or path issues


## to do
- Use llamafile kernel
- Adapt to Linux
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