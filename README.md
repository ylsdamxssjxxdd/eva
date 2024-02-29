# 机体
一款轻量的大模型应用软件：机体 (qt5+llama.cpp)

## 特点
- 直观
    - 非常良好的汉化
    - 推理过程、采样过程、工作流程可视化
    - 输出区的内容就是模型的全部现实
- win7
    - 最低支持32位windows7
    - 具有编译到(x86，arm，windows，linux，macos，android，cuda，rocm，vulkan)的潜力
- 轻量
    - 机体没有其它依赖组件，就是一个exe程序
    - 如果是cuda版本则需要安装cuda12版本的工具包 https://developer.nvidia.com/cuda-toolkit-archive
- 多功能
    - 本地模型交互，在线模型交互，多模态，agent，RAG，对外api服务，模型评测
## 快速开始
1. 下载软件
- 本项目release
- 百度网盘 https://pan.baidu.com/s/18NOUMjaJIZsV_Z0toOzGBg?pwd=body
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
## 源码编译
1. 配置环境
先在系统环境变量中添加对应的qt5库
- 64bit版本
    - 我的工具链为mingw81_64+qt5.15.10(静态编译)
- cuda版本
    - 我的工具链为msvc2022+qt5.15.10+cuda12
- 32bit版本
    - 我的工具链为mingw81_32+qt5.15.10(静态编译)
2. 克隆源代码
```bash
git clone --recurse-submodules https://github.com/ylsdamxssjxxdd/eva.git
```
```bash
cd eva
```
## 待办事项
## 已知BUG
## 运行原理