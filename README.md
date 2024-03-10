# 机体
一款轻量的大模型应用软件：机体 (qt5+llama.cpp)

## 特点
- 直观
    - 运行流程可视化
    - 输出区的内容就是模型的全部现实
- win7
    - 最低支持32位windows7
    - 具有编译到(x86，arm，windows，linux，macos，android，cuda，rocm，vulkan)的潜力
- 轻量
    - 机体没有其它依赖组件，就是一个exe程序
- 多功能
    - 本地模型交互，在线模型交互(待完善)，多模态(待完善)，agent(待完善)，RAG(待完善)，对外api服务，模型评测
    
## 快速开始
1. 下载软件
- 本项目release或者百度网盘 https://pan.baidu.com/s/18NOUMjaJIZsV_Z0toOzGBg?pwd=body
2. 下载模型
- 前往这个网站挑选gguf模型文件 https://hf-mirror.com/
- 百度网盘 https://pan.baidu.com/s/18NOUMjaJIZsV_Z0toOzGBg?pwd=body
3. 装载！
- 点击装载按钮，选择一个gguf模型文件载入内存
4. 发送！
- 在输入区输入聊天内容，点击发送

## 功能介绍
1. 补完模式
2. 对话模式
3. 服务模式
4. 链接模式
5. 挂载工具
6. 扩展窗口

## 源码编译
1. 配置环境
---记得先在系统环境变量中添加对应的qt5库
- 64bit版本
    - 我的工具链为mingw81_64+qt5.15.10(静态库)
- 32bit版本
    - 我的工具链为mingw81_32+qt5.15.10(静态库)
- opencl版本
    - 我的工具链为mingw81_64+clblast(静态库)+qt5.15.10(静态库) 
    - 下载opencl-sdk，将其include目录添加到环境变量  https://github.com/KhronosGroup/OpenCL-SDK
    - 下载clblast，build_shared_lib选项关闭，静态编译并添加到环境变量 https://github.com/CNugteren/CLBlast   
- cuda版本
    - 我的工具链为msvc2022+qt5.15.10+cuda12
    - 安装cuda-tooklit https://developer.nvidia.com/cuda-toolkit-archive
    - 安装cudnn https://developer.nvidia.com/cudnn

2. 克隆源代码
```bash
git clone --recurse-submodules https://github.com/ylsdamxssjxxdd/eva.git
```
3. 根据需要修改CMakeLists.txt文件中的配置

## 运行原理
- 装载流程
    - ...
- 约定流程
    - ...
- 设置流程
    - ...
- 重置流程
    - ...
- 发送流程
    - ...
- 工具调用流程
    - ...    
- 链接流程
    - ...
- 量化流程
    - ...

## 待办事项
在扩展窗口增加模型/工具/图像/声音/视频增殖选项
- 工具增殖
    - 知识库工具
    - 搜素引擎工具
    - 大模型工具
- 模型增殖
    - 模型训练
    - 模型下载
    - 模型转化
    - 模型量化
    - 模型评估
    - 模型微调
- 图像增殖
    - 借助stable-diffusion.cpp项目进行文生图

## 已知BUG
- csv文件存在特殊符号时不能正确解析，待修复
- 达到最大上下文后截断一次后再达到，解码会失败，通过暂时置入空的记忆来缓解，定位在llama_decode提示找不到kv槽，待修复
- 链接模式下，法无间隔连续发送，通过定时100ms后触发来缓解，定位在QNetworkAccessManager不能及时释放，待修复
- 挂载yi-vl视觉后模型输出异常，待修复
---
- 部分字符utf-8解析有问题，已修复（模型输出不完整的utf8字符，需要手动将3个拼接成1个）
- 切换模型时显存泄露，已修复（使用cuda时，不使用mmp）
- 服务模式点击设置就会先终止server.exe而不是点ok后，已修复（将全局的运行线程变为局部变量）