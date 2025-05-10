#ifndef XCONFIG_H
#define XCONFIG_H

#include <samplerate.h>  // 音频重采样
#include <sndfile.h>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QColor>
#include <QCoreApplication>
#include <QOperatingSystemVersion>
#include <QDebug>
#include <array>
#include <QThread>
#include <iostream>
#include <thread>
#include <vector>
#include <QProcessEnvironment>
#include "./src/utils/cmakeconfig.h"
#include "mcp_message.h"

//默认约定
#define DEFAULT_DATE_PROMPT "You are a helpful assistant."
#define DEFAULT_SYSTEM_NAME "system"
#define DEFAULT_USER_NAME "user"
#define DEFAULT_MODEL_NAME "assistant"
#define DEFAULT_OBSERVATION_NAME "tool"
#define DEFAULT_OBSERVATION_STOPWORD "<|observation|>"
#define DEFAULT_THINK_BEGIN "<think>"
#define DEFAULT_THINK_END "</think>"
#define DEFAULT_SPLITER "\n"                 // 分隔符

//采样
#define DEFAULT_NPREDICT 4096
#define DEFAULT_TEMP 0.7
#define DEFAULT_REPEAT 1.2
#define DEFAULT_TOP_P 0.95 // 默认top_p值为0.95

//推理
#define DEFAULT_NTHREAD 1 // 默认线程数为1，但是后面会变的
#define DEFAULT_SPECIAL true // 默认显示特殊token
#ifdef BODY_USE_32BIT
#define DEFAULT_USE_MMAP false // 默认关闭内存映射
#define DEFAULT_FLASH_ATTN false // 默认开启注意力加速
#define DEFAULT_NCTX 256 // 默认上下文长度
#define DEFAULT_BATCH 128 // 默认虚拟批处理大小
#define DEFAULT_UBATCH 128 // 默认物理批处理大小
#else 
#define DEFAULT_NCTX 4096 // 默认上下文长度
#define DEFAULT_BATCH 2048 // 默认虚拟批处理大小
#define DEFAULT_UBATCH 512 // 默认物理批处理大小
#define DEFAULT_USE_MMAP false // 默认关闭内存映射
#define DEFAULT_FLASH_ATTN true // 默认开启注意力加速
#endif
#define DEFAULT_USE_MLOCCK false // 默认关闭内存锁定

#ifdef BODY_USE_GPU // 如果支持GPU则默认设置为最大
#define DEFAULT_NGL 99
#else
#define DEFAULT_NGL 0
#endif
#define DEFAULT_SERVER_PORT "8080"  // 默认服务端口
#define DEFAULT_EMBEDDING_PORT "7758"  // 默认嵌入端口
#define DEFAULT_EMBEDDING_IP "127.0.0.1" // 嵌入端点ip
#define DEFAULT_EMBEDDING_API "/v1/embeddings"  // 嵌入端点地址
#define DEFAULT_EMBEDDING_SPLITLENTH 300
#define DEFAULT_EMBEDDING_OVERLAP 20
#define DEFAULT_EMBEDDING_RESULTNUMB 3
#define MAX_INPUT 8000 //一次最大输入字符数

// llama日志信号字样，用来指示下一步动作
#define SERVER_START "server is listening on"  // server启动成功返回的字样
#define SERVER_EMBD_INFO "print_info: n_embd           = "  // 模型装载成功返回的词嵌入维度字样

//默认的模型路径
#define DEFAULT_LLM_MODEL_PATH "/EVA_MODELS/大语言模型/Qwen2.5-7B-Q3_K_M.gguf"
#define DEFAULT_SD_MODEL_PATH "/EVA_MODELS/文生图模型/sd1.5-anything-3/sd1.5-anything-3-q8_0.gguf"
#define DEFAULT_WHISPER_MODEL_PATH "/EVA_MODELS/声转文模型/whisper-base-q5_1.bin"
#define DEFAULT_OUTETTS_MODEL_PATH "/EVA_MODELS/文转声模型/OuteTTS-0.2-500M-Q8_0.gguf"
#define DEFAULT_WAVTOKENIZER_MODEL_PATH "/EVA_MODELS/文转声模型/WavTokenizer-Large-75-F16.gguf"

//不同操作系统相关
#ifdef _WIN32
#define SFX_NAME ".exe"  // 第三方程序后缀名
#define DEFAULT_SHELL "cmd.exe"
#define DEFAULT_PYTHON "python"
#elif __linux__
#define SFX_NAME ""  //第三方程序后缀名
#define DEFAULT_SHELL "/bin/sh"
#define DEFAULT_PYTHON "python3"
#endif
namespace mcp {

// Use the nlohmann json library
using json = nlohmann::ordered_json;
};
//约定内容
struct DATES {
    QString date_prompt = DEFAULT_DATE_PROMPT;  // 约定指令 影响 系统指令
    QString user_name = DEFAULT_USER_NAME;      // 用户昵称 影响 输入前缀
    QString model_name = DEFAULT_MODEL_NAME;    // 模型昵称 影响 输入后缀
    bool is_load_tool = false;                  // 是否挂载了工具
    QStringList extra_stop_words = {};          //额外停止标志
};

// 经过模型自带模板格式化后的内容
struct CHATS {
    QString system_prompt;  // 系统指令
    QString input_prefix;   // 输入前缀
    QString input_suffix;   // 输入后缀
    QString tool_prefix;   // 输入前缀（工具）
};

//发送内容的源
enum ROLE {
    ROLE_USER,         // 加前缀后缀输入
    ROLE_OBSERVATION,  // 只加后缀，引导模型回答
};

//传递的前缀/输入/后缀
struct INPUTS {
    QString input;
    ROLE role;
};

//颜色
const QColor BODY_WHITE(255, 255, 240);       // 乳白色
const QColor SIGNAL_BLUE(0, 0, 255);       // 蓝色
const QColor SYSTEM_BLUE(0, 0, 255, 200);  // 蓝紫色
const QColor TOOL_BLUE(0, 191, 255);       // 天蓝色
const QColor NORMAL_BLACK(0, 0, 0);        // 黑色
const QColor LCL_ORANGE(255, 165, 0);      // 橘黄色
const QColor THINK_GRAY(128, 128, 128); // 灰色

//机体模式枚举
enum EVA_MODE {
    LOCAL_MODE,  //本地模式
    LINK_MODE,   //链接模式
};

//机体状态枚举
enum EVA_STATE {
    CHAT_STATE,      //对话状态
    COMPLETE_STATE,  //补完状态
    SERVER_STATE,    //服务状态
};

//增殖窗口枚举
enum EXPEND_WINDOW {
    INTRODUCTION_WINDOW,      //软件介绍窗口
    MODELINFO_WINDOW,         //模型信息窗口
    MODELCONVERT_WINDOW,         //模型转换窗口
    QUANTIZE_WINDOW,          //模型量化窗口
    MCP_WINDOW,          //MCP服务器窗口
    KNOWLEDGE_WINDOW,          //知识库窗口
    TXT2IMG_WINDOW,          //文生图窗口
    WHISPER_WINDOW,          //声转文窗口
    TTS_WINDOW,             //文转声窗口
    NO_WINDOW,             //关闭窗口
    PREV_WINDOW,             //上一次的窗口
};

//窗口索引
const QMap<EXPEND_WINDOW, int> window_map = {{INTRODUCTION_WINDOW,0},{MODELINFO_WINDOW,1},{MODELCONVERT_WINDOW,2},{QUANTIZE_WINDOW,3},{MCP_WINDOW,4},{KNOWLEDGE_WINDOW,5},{TXT2IMG_WINDOW,6},{WHISPER_WINDOW,7},{TTS_WINDOW,8},{NO_WINDOW,999},{PREV_WINDOW,-1}};

//模型类型枚举
enum MODEL_TYPE {
    MODEL_TYPE_LLM,      //大语言模型
    MODEL_TYPE_WHISPER,      //WHISPER模型
    MODEL_TYPE_SD,      //SD模型
    MODEL_TYPE_OUTETTS,      //OUTETTS模型
};

//模型量化级别枚举
enum MODEL_QUANTIZE {
    MODEL_QUANTIZE_F32,
    MODEL_QUANTIZE_BF16,
    MODEL_QUANTIZE_F16,
    MODEL_QUANTIZE_Q8_0,
};

//模型转换脚本
#define CONVERT_HF_TO_GGUF_SCRIPT "convert_hf_to_gguf.py"

const QMap<MODEL_TYPE, QString> modeltype_map = {{MODEL_TYPE_LLM,"llm"},{MODEL_TYPE_WHISPER,"whisper"},{MODEL_TYPE_SD,"sd"},{MODEL_TYPE_OUTETTS,"outetts"}};
const QMap<MODEL_QUANTIZE, QString> modelquantize_map = {{MODEL_QUANTIZE_F32,"f32"},{MODEL_QUANTIZE_F16,"f16"},{MODEL_QUANTIZE_BF16,"bf16"},{MODEL_QUANTIZE_Q8_0,"q8_0"}};

//设置参数
struct SETTINGS {
    double temp = DEFAULT_TEMP;
    double repeat = DEFAULT_REPEAT;
    int ngl = DEFAULT_NGL;
    int nctx = DEFAULT_NCTX;
    int nthread = std::thread::hardware_concurrency() * 0.5;
    QString modelpath = "";
    QString lorapath = "";
    QString mmprojpath = "";
    bool complete_mode = false;

    //隐藏的设置
    int hid_npredict = DEFAULT_NPREDICT;
    bool hid_special           = DEFAULT_SPECIAL; // enable special token output
    double hid_top_p = DEFAULT_TOP_P;
    int hid_batch = DEFAULT_BATCH;
    int32_t hid_n_ubatch              =   DEFAULT_UBATCH; // physical batch size for prompt processing (must be >=32 to use BLAS)
    bool hid_use_mmap          = DEFAULT_USE_MMAP;  // use mmap for faster loads
    bool hid_use_mlock         = DEFAULT_USE_MLOCCK; // use mlock to keep model in memory
    bool hid_flash_attn        = DEFAULT_FLASH_ATTN; // flash attention

};

//模型参数,模型装载后发送给ui的参数
struct MODEL_PARAMS {
    int n_ctx_train;  // 最大上下文长度
    int max_ngl;      // ngl的最大值
};

#define CHAT_ENDPOINT "/v1/chat/completions"
#define COMPLETION_ENDPOINT "/completions"

// api配置参数
struct APIS {
    QString api_endpoint = "";  // openai格式端点 = ip + port
    QString api_key = "";
    QString api_model = "default";
    QString api_chat_endpoint = CHAT_ENDPOINT;
    QString api_completion_endpoint = COMPLETION_ENDPOINT;
    bool is_cache = true;
};

// 链接模式下发送消息的对象枚举
enum API_ROLE {
    API_ROLE_USER,
    API_ROLE_ASSISANT,
    // API_ROLE_OBSERVATION, // 暂时不会用
};

//端点接收参数
struct ENDPOINT_DATA {
    QString date_prompt;
    QString input_pfx;
    QString input_sfx;
    QString input_prompt;                              //续写模式用
    QVector<QPair<QString, API_ROLE>> insert_history;  // 将要构造的历史数据，前面是内容后面是角色
    bool complete_state;
    float temp;
    double repeat;
    int n_predict;
    QStringList stopwords;  //停止标志
};

//单参数工具
struct TOOLS {
    QString tool_name;      //工具名
    QString func_name;      //函数名
    QString func_describe;  //功能描述
};

struct TOOLS_INFO {
    QString name;      // 工具名
    QString description; // 功能描述
    QString arguments;  // 参数结构
    QString text;     // 信息文本

    void generateToolText() {
        // 使用QString格式化将三个字段合并为指定格式
        text = QString("{\"name\":\"%1\",\"description\":\"%2\",\"arguments\":%3}")
                  .arg(name)       // 插入工具名
                  .arg(description)  // 插入功能描述
                  .arg(arguments);  // 插入参数结构
    }

    // 手动实现默认构造函数
    TOOLS_INFO() {} // 空实现

    // 构造函数
    TOOLS_INFO(const QString& n, const QString& desc, const QString& params) {
        name = n;
        description = desc;
        arguments = params;
        generateToolText(); // 自动生成text字段
    }


};

//MCP连接状态枚举
enum MCP_CONNECT_STATE{
    MCP_CONNECT_LINK, // 正常连接
    MCP_CONNECT_WIP, // 等待重连
    MCP_CONNECT_MISS,// 未连接
};

inline std::vector<TOOLS_INFO> MCP_TOOLS_INFO_LIST; // 保存所有用户要用的mcp工具，全局变量
inline mcp::json MCP_TOOLS_INFO_ALL;//当前服务可用的所有工具，全局变量

//状态区信号枚举
enum SIGNAL_STATE {
    USUAL_SIGNAL,    //一般输出，黑色
    SIGNAL_SIGNAL,   //信号，蓝色
    SUCCESS_SIGNAL,  //成功，绿色
    WRONG_SIGNAL,    //错误，红色
    EVA_SIGNAL,      //机体，紫色
    TOOL_SIGNAL,     //工具，天蓝色
    SYNC_SIGNAL,     //同步，橘黄色
    MATRIX_SIGNAL,   //文本表格，黑色，不过滤回车符
};

// whisper可以传入的参数
struct Whisper_Params {
    int32_t n_threads = 1;
    int32_t n_processors = 1;
    int32_t offset_t_ms = 0;
    int32_t offset_n = 0;
    int32_t duration_ms = 0;
    int32_t progress_step = 5;
    int32_t max_context = -1;
    int32_t max_len = 0;
    // int32_t best_of      = whisper_full_default_params(WHISPER_SAMPLING_GREEDY).greedy.best_of;
    // int32_t beam_size    = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH).beam_search.beam_size;
    int32_t audio_ctx = 0;

    float word_thold = 0.01f;
    float entropy_thold = 2.40f;
    float logprob_thold = -1.00f;

    bool speed_up = false;
    bool debug_mode = false;
    bool translate = false;
    bool detect_language = false;
    bool diarize = false;
    bool tinydiarize = false;
    bool split_on_word = false;
    bool no_fallback = false;
    bool output_txt = false;
    bool output_vtt = false;
    bool output_srt = false;
    bool output_wts = false;
    bool output_csv = false;
    bool output_jsn = false;
    bool output_jsn_full = false;
    bool output_lrc = false;
    bool no_prints = false;
    bool print_special = false;
    bool print_colors = false;
    bool print_progress = false;
    bool no_timestamps = false;
    bool log_score = false;
    bool use_gpu = true;

    std::string language = "zh";
    std::string prompt;
    std::string font_path = "/System/Library/Fonts/Supplemental/Courier New Bold.ttf";
    std::string model = "";

    // [TDRZ] speaker turn string
    std::string tdrz_speaker_turn = " [SPEAKER_TURN]";  // TODO: set from command line

    std::string openvino_encode_device = "CPU";

    std::vector<std::string> fname_inp = {};
    std::vector<std::string> fname_out = {};
};

struct Embedding_Params {
    QString modelpath = "";
};

// 定义一个结构体来存储索引和值
struct Embedding_vector {
    int index;  // 用于排序的序号，在表格中的位置
    QString chunk;
    std::vector<double> value;  // 支持任意维度向量
};

//量化方法说明数据结构
struct QuantizeType {
    QString typename_;   //方法名
    QString bit;         //压缩率,fp16为基准
    QString perplexity;  //困惑度
    QString recommand;   //推荐度
};

//文转声参数
struct Speech_Params {
    bool enable_speech = false;
    QString speech_name = "";
};

#define SPPECH_OUTETTS "outetts"

//记忆单元（当前记忆）
struct Brain_Cell {
    int id;        //在缓存中的序号
    int token;     //缓存的词索引
    QString word;  //词索引对应的词
};


// 文生图参数
#define DEFAULT_SD_NOISE "0.75"  //噪声系数

struct SD_PARAMS {
    QString sample_type;      //采样算法euler, euler_a, heun, dpm2, dpm++2s_a, dpm++2m, dpm++2mv2, lcm
    QString negative_prompt;  //反向提示词
    QString modify_prompt;    //修饰词
    int width;                //图像宽度
    int height;               //图像高度
    int steps;                //采样步数
    int batch_count;          //出图张数
    int seed;                 //随机数种子 -1随机
    int clip_skip;            //跳层
    double cfg_scale;         //提示词与图像相关系数

    // 构造函数
    SD_PARAMS(QString sample_type = "euler", QString negative_prompt = "", QString modify_prompt = "", int width = 512, int height = 512, int steps = 20, int batch_count = 1, int seed = -1, int clip_skip = -1, double cfg_scale = 7.5) : sample_type(sample_type), negative_prompt(negative_prompt), modify_prompt(modify_prompt), width(width), height(height), steps(steps), batch_count(batch_count), seed(seed), clip_skip(clip_skip), cfg_scale(cfg_scale) {}
};

// 对音频重采样为16khz
inline bool resampleWav(const std::string& inputPath, const std::string& outputPath) {
    SF_INFO inputFileInfo;
    SF_INFO outputFileInfo;
    SNDFILE* inputFile = sf_open(inputPath.c_str(), SFM_READ, &inputFileInfo);

    if (!inputFile) {
        std::cerr << "Error opening input file: " << sf_strerror(nullptr) << std::endl;
        return false;
    }

    int channels = inputFileInfo.channels;
    int inputSampleRate = inputFileInfo.samplerate;
    int outputSampleRate = 16000;

    std::vector<float> inputBuffer(inputFileInfo.frames * channels);
    sf_readf_float(inputFile, inputBuffer.data(), inputFileInfo.frames);

    double ratio = static_cast<double>(outputSampleRate) / inputSampleRate;
    int outputFrames = static_cast<int>(inputFileInfo.frames * ratio);
    std::vector<float> outputBuffer(outputFrames * channels);

    SRC_DATA srcData;
    srcData.data_in = inputBuffer.data();
    srcData.input_frames = inputFileInfo.frames;
    srcData.data_out = outputBuffer.data();
    srcData.output_frames = outputFrames;
    srcData.src_ratio = ratio;
    srcData.end_of_input = SF_TRUE;

    int srcError = src_simple(&srcData, SRC_SINC_MEDIUM_QUALITY, channels);
    if (srcError != 0) {
        std::cerr << "Error during resampling: " << src_strerror(srcError) << std::endl;
        sf_close(inputFile);
        return false;
    }

    outputFileInfo.channels = channels;
    outputFileInfo.samplerate = outputSampleRate;
    outputFileInfo.format = inputFileInfo.format;

    SNDFILE* outputFile = sf_open(outputPath.c_str(), SFM_WRITE, &outputFileInfo);
    if (!outputFile) {
        std::cerr << "Error opening output file: " << sf_strerror(nullptr) << std::endl;
        sf_close(inputFile);
        return false;
    }

    sf_writef_float(outputFile, outputBuffer.data(), srcData.output_frames_gen);

    sf_close(inputFile);
    sf_close(outputFile);

    return true;
}

inline QString parseFirstKeyValue(const QString& jsonString) {
    QByteArray jsonData = jsonString.toUtf8();
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        // qDebug() << "JSON 解析错误：" << parseError.errorString();
        return jsonString;
    }
    
    QJsonObject jsonObj = jsonDoc.object();
    QStringList keys = jsonObj.keys();
    
    if (!keys.isEmpty()) {
        QString firstKey = keys.first();
        if (jsonObj[firstKey].isString()) {
            return jsonObj[firstKey].toString();
        }
    }
    
    return jsonString;
}

inline QString getLinuxOSName() {
    QFile file("/etc/os-release");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    QTextStream in(&file);
    QString osName;
    QString osVersion;

    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.startsWith("NAME=")) {
            osName = line.section('=', 1, 1).remove('"');
        } else if (line.startsWith("VERSION_ID=")) {
            osVersion = line.section('=', 1, 1).remove('"');
        }
    }

    file.close();
    return osName + " " + osVersion;
}
//操作系统版本
#ifdef Q_OS_LINUX
const QString USEROS = getLinuxOSName();
const QString CMDGUID = "-c";
#else
const QString CMDGUID = "/c";
const QString USEROS = QOperatingSystemVersion::current().name() + " " + QString::number(QOperatingSystemVersion::current().majorVersion());
#endif

inline void createDesktopShortcut(QString appPath) {
    // 仅在 Linux 环境下执行
#ifdef Q_OS_LINUX
    // 准备图标路径：将图标从资源文件复制到用户目录
    QString iconDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/icons/";// 指向~/.local/share/icons
    QDir().mkpath(iconDir); // 确保目录存在
    QString iconPath = iconDir + "eva.png";
    QFile::copy(":/logo/blue_logo.png", iconPath);
    QFile::setPermissions(iconPath, QFile::ReadOwner | QFile::WriteOwner);

    // 构造 .desktop 文件内容（处理路径中的空格）
    QString desktopContent = QString(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=%1\n"
        "Comment=a lite llm tool\n"
        "Exec=%2\n"
        "Icon=%3\n"
        "Terminal=false\n"
        "Categories=Utility\n"
    ).arg(EVA_VERSION, appPath, iconPath);

    // 写入 ~/.local/share/applications/eva.desktop
    QString applicationsDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) + "/";// 指向~/.local/share/applications
    QDir().mkpath(applicationsDir); // 确保目录存在
    QFile applicationsFile(applicationsDir + "eva.desktop");
    if (applicationsFile.open(QIODevice::WriteOnly)) {
        applicationsFile.write(desktopContent.toUtf8());
        applicationsFile.close();
        applicationsFile.setPermissions(QFile::ExeOwner | QFile::ReadOwner | QFile::WriteOwner);
    } else {
        qWarning() << "Failed to write applications desktop file";
    }

    // 写入 ~/Desktop/eva.desktop
    QString desktopDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/";
    QFile desktopFile(desktopDir + "eva.desktop");
    if (desktopFile.open(QIODevice::WriteOnly)) {
        desktopFile.write(desktopContent.toUtf8());
        desktopFile.close();
        desktopFile.setPermissions(QFile::ExeOwner | QFile::ReadOwner | QFile::WriteOwner);
    } else {
        qWarning() << "Failed to write desktop shortcut";
    }
#endif
}

// 安全获取字符串列表（原函数增强版，支持任意键）
inline std::vector<std::string> get_string_list_safely(const mcp::json& json_, const std::string& key) {
    std::vector<std::string> result;
    try {
        result = json_.value(key, mcp::json::array()).get<std::vector<std::string>>();
    } catch (const mcp::json::exception&) {
        result.clear();
    }
    return result;
}

// 安全获取整数列表
inline std::vector<int> get_int_list_safely(const mcp::json& json_, const std::string& key) {
    std::vector<int> result;
    try {
        result = json_.value(key, mcp::json::array()).get<std::vector<int>>();
    } catch (const mcp::json::exception&) {
        result.clear();
    }
    return result;
}

// 安全获取JSON对象（返回mcp::json对象类型）
inline mcp::json get_json_object_safely(const mcp::json& json_, const std::string& key) {
    try {
        auto obj = json_.value(key, mcp::json::object());
        return obj.is_object() ? obj : mcp::json::object();
    } catch (const mcp::json::exception&) {
        return mcp::json::object();
    }
}

// 安全获取字符串（支持默认值）
inline std::string get_string_safely(const mcp::json& json_, const std::string& key, const std::string& default_val = "") {
    try {
        const mcp::json& value = json_.value(key, mcp::json(default_val));
        if (value.is_string()) {
            return value.get<std::string>();
        } else if (value.is_number()) {
            return value.dump(); // 将数值转换为字符串形式
        } else {
            return default_val;
        }
    } catch (const mcp::json::exception&) {
        return default_val;
    }
}

// 安全获取整数（支持默认值）
inline int get_int_safely(const mcp::json& json_, const std::string& key, int default_val = 0) {
    try {
        return json_.value(key, mcp::json(default_val)).get<int>();
    } catch (const mcp::json::exception&) {
        return default_val;
    }
}

// 安全获取双精度浮点数（支持默认值）
inline double get_double_safely(const mcp::json& json_, const std::string& key, double default_val = 0.0) {
    try {
        return json_.value(key, mcp::json(default_val)).get<double>();
    } catch (const mcp::json::exception&) {
        return default_val;
    }
}

// 安全获取布尔值（支持默认值）
inline bool get_bool_safely(const mcp::json& json_, const std::string& key, bool default_val = false) {
    try {
        return json_.value(key, mcp::json(default_val)).get<bool>();
    } catch (const mcp::json::exception&) {
        return default_val;
    }
}

// 安全获取通用JSON数组（元素类型为mcp::json）
inline std::vector<mcp::json> get_json_array_safely(const mcp::json& json_, const std::string& key) {
    try {
        return json_.value(key, mcp::json::array()).get<std::vector<mcp::json>>();
    } catch (const mcp::json::exception&) {
        return {};
    }
}

// 内置的回复工具
static TOOLS_INFO Buildin_tools_answer(
    "answer",
    "The final response to the user",
    "{""\"type\":\"object\",""\"properties\":{""\"content\":{""\"type\":\"string\",""\"description\":\"The final response to the user\"""}""},""\"required\":[\"content\"]""}"
);

// 内置的计算器工具
static TOOLS_INFO Buildin_tools_calculator(
    "calculator",
    "Enter a expression to return the calculation result via using tinyexpr",
    "{""\"type\":\"object\",""\"properties\":{""\"expression\":{""\"type\":\"string\",""\"description\":\"math expression\"""}""},""\"required\":[\"expression\"]""}"
);

// 内置的软件控制台工具
static TOOLS_INFO Buildin_tools_controller(
    "controller",
    "Pass in the corresponding numbers to control the software",
    "{""\"type\":\"object\",""\"properties\":{""\"number\":{""\"type\":\"int\",""\"description\":\"1(Maximize the main window); 2(Minimize the main window); 3(Top the main window); 4(Cancel the top of the main window); 5(Close the main window); 6(Play music); 7(Close music); 8(Open the proliferation window); 9(Close the proliferation window)\"""}""},""\"required\":[\"number\"]""}"
);

// 内置的列出MCP工具
static TOOLS_INFO Buildin_tools_mcp_tools_list(
    "mcp_tools_list",
    "List all available tools in the current MCP service, which may include tools that you can use",
    "{""\"type\":\"object\",""\"properties\":{""""}""}"
);

// 内置的知识库工具
static TOOLS_INFO Buildin_tools_knowledge(
    "knowledge",
    "Ask a question to the knowledge base, the more detailed the question, the better. The knowledge base will return three text segments with the highest similarity to the question. knowledge database describe: {embeddingdb describe}",
    "{""\"type\":\"object\",""\"properties\":{""\"content\":{""\"type\":\"string\",""\"description\":\"Keywords to be queried\"""}""},""\"required\":[\"content\"]""}"
);

// 内置的文生图工具
static TOOLS_INFO Buildin_tools_stablediffusion(
    "stablediffusion",
    "Describe the image you want to draw with a paragraph of English text. The tool will send the text to the drawing model and then return the drawn image, making sure to input English. You can add modifiers or phrases to improve the quality of the image before the text and separate them with commas.",
    "{""\"type\":\"object\",""\"properties\":{""\"prompt\":{""\"type\":\"string\",""\"description\":\"Describe the image you want to draw with a paragraph of English text\"""}""},""\"required\":[\"prompt\"]""}"
);

// 内置的工程师-命令行工具
static TOOLS_INFO Buildin_tools_execute_command(
    "execute_command",
    "Request to execute CLI commands on the system. Use this command when you need to perform system operations or run specific commands to complete any step of a user task. You must adjust the commands according to the user's system. Prioritize executing complex CLI commands over creating executable scripts, as they are more flexible and easier to run. The command will be executed in the current working directory.",
    "{""\"type\":\"object\",""\"properties\":{""\"content\":{""\"type\":\"string\",""\"description\":\"CLI commands\"""}""},""\"required\":[\"content\"]""}"
);

// 内置的工程师-读文件工具
static TOOLS_INFO Buildin_tools_read_file(
    "read_file",
    "Request to read the content of a file in a specified path, used when you need to check the content of an existing file, such as analyzing code, reviewing text files, or extracting information from a configuration file.",
    "{""\"type\":\"object\",""\"properties\":{""\"path\":{""\"type\":\"string\",""\"description\":\"The file path which you want to read\"""}""},""\"required\":[\"path\"]""}"
);

// 内置的工程师-写文件工具
static TOOLS_INFO Buildin_tools_write_file(
    "write_file",
    "Request to write content to a file at the specified path. If the file exists, it will be overwritten by the provided content. If the file does not exist, it will be created. This tool will automatically create any directory required for writing files.",
    "{""\"type\":\"object\",""\"properties\":{""\"path\":{""\"type\":\"string\",""\"description\":\"The file path which you want to write\"""},\"content\":{""\"type\":\"string\",""\"description\":\"The file content\"""}""},""\"required\":[\"path\",\"content\"]""}"
);


#endif XCONFIG_H


