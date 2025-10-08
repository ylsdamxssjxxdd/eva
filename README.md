# 机体

一款流畅的llama.cpp启动器

\[ 中文 | [English](README_en.md) \]


<img src="https://github.com/ylsdamxssjxxdd/eva/assets/63994076/a7c5943a-aa4f-4e46-a6c6-284be990fd59" width="300px">


## 特点

- 易用的操作界面 🧮

- 原生的运行效率 🚀

- 直观的推理流程 👀

- 全面的模型支援 🐳

- 简洁的智能体实现 🤖

- 丰富的增殖功能 🐣


## 快速开始

1. 下载一个机体

    - https://pan.baidu.com/s/18NOUMjaJIZsV_Z0toOzGBg?pwd=body

    ```txt
        windows下载 .exe 后缀的程序，linux下载 .AppImage 后缀的程序
        
        其中cpu版本使用avx加速，兼容性较好；cuda版本使用nvidia显卡加速；vulkan版本可以使用任意显卡加速，速度不如cuda版本
        
        linux下需要在终端中输入这个命令以使机体获得运行权限：chmod 777 ***.AppImage 
        最好将.AppImage放到一个稳定的纯英文路径中，只要运行一次.AppImage就会自动配置桌面快捷方式和开始菜单
    ```

2. 下载一个gguf格式模型

    - https://pan.baidu.com/s/18NOUMjaJIZsV_Z0toOzGBg?pwd=body

    - 也可以前往 https://hf-mirror.com 搜索，机体支持几乎所有开源大语言模型

3. 装载！

    - 点击装载按钮，选择一个gguf模型载入内存

4. 发送！

    - 在输入区输入聊天内容，点击发送

5. 加速！

    - 点击设置，调整gpu负载层数，显存充足建议拉满，注意显存占用超过95%的话会很卡

    - 同时运行sd的话要确保给sd留足显存


## 功能

### 基础功能

<details>

<summary> 两种模式 </summary>

1. 本地模式：用户左键单击装载按钮，通过装载本地的模型进行交互

2. 链接模式：用户右键单击装载按钮，输入某个模型服务的api端点进行交互（目前支持openai类型兼容接口）

</details>

<details>

<summary> 两种状态 </summary>

1. 对话状态

    - 机体的默认状态，在输入区输入聊天内容，模型进行回复

    - 可以事先约定好角色

    - 可以使用挂载的工具

    - 可以按f1截图，按f2进行录音，截图和录音会发送给多模态或whisper模型进行相应处理

2. 补完状态

    - 在输出区键入任意文字，模型对其进行补完

 

</details>

<details>

<summary> 六个工具 </summary>

在 本地模式 + 对话状态 下，用户可以点击约定为模型挂载工具

```txt
    原理是在系统指令中添加一段额外的指令来指导模型调用相应的工具
    每当模型预测结束后，机体自动检测其是否包含调用工具的xml字段，若有则调用相应的工具，工具执行完毕后将结果再发送给模型继续进行预测
```

1. 计算器

    - 模型输出计算公式给计算器工具，工具将返回计算结果

    - 例如：计算888*999 

    - 调用难度：⭐

2. 鼠标键盘

    - 模型输出行动序列来控制用户的鼠标和键盘，需要模型拥有视觉才能完成定位

    - 例如：帮我自动在冒险岛里搬砖

    - 调用难度：⭐⭐⭐⭐⭐

3. 软件工程师

    - 类似cline的自动化工具执行链

    - 例如：帮我构建一个cmake qt的初始项目

    - 调用难度：⭐⭐⭐⭐⭐

4. 知识库

    - 模型输出查询文本给知识库工具，工具将返回三条最相关的已嵌入知识

    - 要求：用户需要先在增殖窗口上传文档并构建知识库

    - 例如：请问机体有哪些功能？

    - 调用难度：⭐⭐⭐

    <img src="https://github.com/ylsdamxssjxxdd/eva/assets/63994076/a0b8c4e7-e8dd-4e08-bcb2-2f890d77d632" width="500px">

5. 文生图

    - 模型输出绘画提示词给文生图工具，工具将返回绘制好的图像

    - 要求：用户需要先在增殖窗口配置文生图的模型路径，支持sd和flux模型

    - 例如：画一个女孩

    - 调用难度：⭐⭐

    <img src="https://github.com/ylsdamxssjxxdd/eva/assets/63994076/627e5cd2-2361-4112-9df4-41b908fb91c7" width="500px">

6. MCP工具

    - 通过MCP服务，获取到外部丰富的工具

    - 说明：挂载工具后需要前往增殖窗口配置MCP服务

    - 调用难度：⭐⭐⭐⭐⭐

</details>

### 增强功能

https://github.com/user-attachments/assets/d1c7b961-24e0-4a30-af37-9c8daf33aa8a

<details>

<summary> 视觉 </summary>

- 介绍：在 本地模式 + 对话状态 下可以挂载视觉模型，视觉模型一般名称中带有mmproj，并且只和特定的模型相匹配。挂载成功后用户可以选择图像进行预解码，来作为模型的上文

- 激活方法：在设置中右击 "挂载视觉" 的输入框选择mmproj模型。可以通过 拖动图片到输入框 或 右击输入框点击<上传图像> 或 按f1进行截图，然后点击发送按钮对图像进行预解码，解码完毕再进行问答

</details>

<details>

<summary> 听觉 </summary>

- 介绍：借助whisper.cpp项目将用户的声音转为文本，也可以直接传入音频转为字幕文件

- 激活方法：右击状态区打开增殖窗口，选择声转文选项卡，选择whisper模型所在路径。回到主界面按f2快捷键即可录音，再按f2结束录音，并自动转为文本填入到输入区

</details>

<details>

<summary> 语音 </summary>

- 介绍：借助windows系统的语音功能将模型输出的文本转为语音并自动播放，或者可以自己配置outetts模型进行文转声

- 激活方法：右击状态区打开增殖窗口，选择文转声选项卡，选择一个声源并启动。

</details>

### 辅助功能

<details>

<summary> 模型量化 </summary>

- 可以右击状态区弹出增殖窗口，在模型量化选项卡中对未经量化的fp32、fp16、bf16的gguf模型进行量化

</details>

<details>

<summary> 自动监视 </summary>

- 本地对话状态下，挂载视觉后，可以设置监视帧率，模型会自动以这个频率监视屏幕

</details>

## 源码编译

<details>

<summary> 展开 </summary>

1. 配置环境

    - 安装编译器 windows可以用msvc或mingw，linux需要g++或clang

    - 安装Qt5.15库 https://download.qt.io/

    - 安装cmake https://cmake.org/

    - 如果要用nvidia显卡加速，安装cuda-tooklit https://developer.nvidia.com/cuda-toolkit-archive

    - 如果要用各种型号显卡加速，安装VulkanSDK https://vulkan.lunarg.com/sdk/home

2. 克隆源代码

    ```bash
    git clone https://github.com/ylsdamxssjxxdd/eva.git
    ```

3. 编译

    ```bash
    cd eva
    cmake -B build -DBODY_PACK=OFF -DGGML_VULKAN=OFF -DGGML_CUDA=OFF
    cmake --build build --config Release -j 8
    ```

    - BODY_PACK：是否需要打包的标志，若开启，windows下将所有组件放置在bin目录下；linux下将所有组件打包为一个AppImage文件，但是依赖linuxdeploy等工具需要自行配置

    - GGML_CUDA：是否需要启用cuda加速的标志

    - GGML_VULKAN：是否需要启用vulkan加速的标志

</details>

## 行动纲领

<details>

<summary> 展开 </summary>

- 装载（本地模式）

    - 【ui】用户点击装载→选择本地模式→选择模型→预装载 preLoad（清屏/锁定/动画）→【backend】根据当前设置 ensureRunning 启动/重启 llama-server→监听 serverReady(endpoint)→【ui】切换到本地端点、初始化会话（插入系统指令）、收尾动画→unlockLoad→正常界面状态→END

    - 首次装载按可用显存与模型体积粗估 ngl=999/0，装载后以 n_layer+1 修正显示；GPU/CPU 状态定时刷新

- 装载（链接模式）

    - 【ui】用户点击装载→选择链接模式→填写 endpoint/key/model→set_api→切换为 LINK_MODE、停止本地服务与在途请求→清屏并注入系统指令→【net】指向远端端点→正常界面状态→END

- 发送/推理（统一）

    - 【ui】构建 OpenAI 兼容消息（system+user，可含 text/image_url/input_audio）→emit ui2net_data + ui2net_push→【net】POST /v1/chat/completions 或 /completions，SSE 流式解析→net2ui_output 逐片输出，net2ui_state 汇报状态，net2ui_kv_tokens/… 统计用量→完成时 net2ui_pushover→【ui】normal_finish_pushover 收尾、解锁；若启用工具则转入工具调用→END

- 工具调用

    - 【ui】解析 assistant 输出中的工具 XML→emit ui2tool_exec→【tool】按 name 执行（calculator/execute_command/knowledge/controller/stablediffusion/…）→返回结果→【ui】以“tool_response: …”封装到 user 消息并着色显示→自动继续发送→直至无工具请求→END

- 图像/音频输入

    - 图像：拖拽/上传/按 F1 截图→以 {type:image_url} 追加到 user 消息；支持多图

    - 音频：上传 WAV/MP3/OGG/FLAC→UI 临时以 audio_url 表示→【net】发送前转换为 OpenAI input_audio 结构

- 录音转文字（Whisper）

    - 【ui】首次 F2 打开增殖窗口选择 whisper 模型→再次 F2 开始录音→再次 F2 结束→保存 WAV 并重采样 16kHz→【expend】调用 whisper-cli 解码→结果写盘并通过信号回填输入框→END

- 约定与设置

    - 约定：用户点击约定→编辑系统指令/昵称/工具开关→确认→set_date→统一重置上下文 on_reset_clicked→END

    - 设置：用户点击设置→修改参数→若涉及后端（模型/设备/nctx/ngl/lora/mmproj/并发/批等）→【backend】按需重启并在 serverReady 后恢复 UI；仅采样参数变化→不重启，仅重置上下文→END

- 知识库

    - 构建：用户在增殖-知识库选择嵌入模型→【expend】启动嵌入服务（--embedding）→上传/编辑文本→逐段调用 /v1/embeddings 得到向量→入库并同步给【tool】→END

    - 问答：通过“工具调用”中的 knowledge 流程完成（计算查询向量并返回相似度最高的文本段）→END

- 说明

    - 所有推理均经由【net】请求式完成；【backend】仅负责在本地模式下托管 llama-server 进程与端点切换

</details>

## 概念

<details>

<summary> 展开 </summary>

- model（模型）: 由一个公式和一组参数组成

- token（词元）: 词的编号，例如，你好 token=123，我 token=14，他的 token=3249，不同模型编号不一样

- vocab（词表）: 该模型训练时所设置的全部词的token，不同模型词表不一样，词表里中文占比越高的往往中文能力强

- kv cache（上下文缓存）: 先前计算的模型注意力机制的键和值，相当于模型的记忆

- decoding（解码）：模型根据上下文缓存和送入的新token计算出向量表，并得到新的上下文缓存

- sampling（采样）：根据向量表计算出概率表并选出下一个词

- predict（预测）：（解码 + 采样）循环

- predecode（预解码）：只解码不采样，用于缓存上下文如系统指令

---

- n_ctx_train（最大上下文长度）: 该模型训练时能送入解码的最大token数量

- n_ctx（上下文长度）: 用户设置的解码时模型能接受的的最大token数量，不能超过n_ctx_train，相当于记忆容量

- temperature（温度）: 采样时会根据温度值将向量表转为概率表，温度越高随机性越大

- vecb（向量表）: 本次解码中词表里所有token的概率分布

- prob（概率表）: 本次采样中词表里所有token的最终选用概率

</details>

## 后端准备（手动）

- 从上游或第三方获取已编译的推理程序，并在项目根目录创建 `backend/` 目录（与 `CMakeLists.txt` 同级）。
- 在 `backend/<设备名>/` 下放置对应可执行文件（可任意子目录，EVA 会递归查找）：
  - 本地 LLM: `llama-server`（及同目录所需的动态库）
  - 声转文: `whisper-cli`
  - 文生图: `sd`
- 设备名不限（例如 `cpu`、`cuda`、`vulkan`、`opencl`、`mygpu`）。EVA 设置页会自动列出 `backend/` 下的所有一级目录名。
- 构建时 CMake 会将 `backend/` 复制到 `build/bin/backend/`；运行时 EVA 会在该目录下递归定位可执行文件并自动补充库搜索路径（Windows: PATH；Linux: LD_LIBRARY_PATH）。
- Windows 中文路径：EVA 会自动把模型/权重路径转换为第三方后端可接受的 ASCII 别名（8.3 短路径/同盘硬链接/跨盘副本），避免“中文路径打不开”的问题。
