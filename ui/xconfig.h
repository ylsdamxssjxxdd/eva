#ifndef XCONFIG_H
#define XCONFIG_H

#include <QCoreApplication>
#include <QColor>
#include <QDebug>
#include <array>
#include <iostream>
#include <thread>
#include <vector>

//默认约定
#define DEFAULT_DATE_PROMPT "You are a helpful assistant."
#define DEFAULT_USER_NAME "user"
#define DEFAULT_MODEL_NAME "assistant"
#define DEFAULT_SPLITER "\n"  // 分隔符
#define DEFAULT_THOUGHT "thought: "  // 思考词
#define DEFAULT_OBSERVATION "observation: "  // 观察词

//约定内容
struct DATES {
    QString date_prompt = DEFAULT_DATE_PROMPT; // 约定指令 影响 系统指令
    QString user_name = DEFAULT_USER_NAME; // 用户昵称 影响 输入前缀
    QString model_name = DEFAULT_MODEL_NAME; // 模型昵称 影响 输入后缀
    bool is_load_tool = false; // 是否挂载了工具
    QStringList extra_stop_words = {};  //额外停止标志
};

// 经过模型自带模板格式化后的内容
struct CHATS {
    QString system_prompt; // 系统指令
    QString input_prefix; // 输入前缀
    QString input_suffix; // 输入后缀
};

//发送内容的源
enum ROLE {
    ROLE_USER,     // 加前缀后缀输入。     
    ROLE_TOOL,     // 不加前缀后缀，用天蓝色输出用户输入部分。 
    ROLE_TEST,     // 同时改变is_test标志。             
    ROLE_DEBUG,    // 不加前缀后缀输入。
    ROLE_THOUGHT,  // 后缀末尾的分隔符用 thought: 代替   
};

//传递的前缀/输入/后缀
struct INPUTS {
    QString input;
    ROLE role;
};

//采样
#define DEFAULT_NPREDICT 4096
#define DEFAULT_TEMP 0.5
#define DEFAULT_REPEAT 1.2

//推理
#define DEFAULT_NCTX 4096
#define DEFAULT_BATCH 2048

#define DEFAULT_NTHREAD 1
#define DEFAULT_NGL 0

#define DEFAULT_MODELPATH "D:/soul"  // 模型所在文件夹
#define DEFAULT_EMBEDDING_PORT "7758"  // 嵌入端口

// llama日志信号字样，用来指示下一步动作
#define SERVER_START "server is listening on"  // server启动成功返回的字样
#define LLM_EMBD "llm_load_print_meta: n_embd           = "  // 模型装载成功返回的词嵌入维度字样

//颜色
const QColor SIGNAL_BLUE(0, 0, 255);       // 蓝色
const QColor SYSTEM_BLUE(0, 0, 255, 200);  // 蓝紫色
const QColor TOOL_BLUE(0, 191, 255);       // 天蓝色
const QColor NORMAL_BLACK(0, 0, 0);        // 黑色
const QColor LCL_ORANGE(255, 165, 0);      // 橘黄色

//不同操作系统相关
#ifdef _WIN32
#define SFX_NAME ".exe"  // 第三方程序后缀名
#define OS "windows"
#elif __linux__
#define SFX_NAME ""  //第三方程序后缀名
#define OS "linux"
#endif

// 字体
#ifdef _WIN32
#define DEFAULT_FONT "SimSun"
#elif __linux__
#define DEFAULT_FONT "Ubuntu Mono"
#endif

//机体模式枚举
enum EVA_MODE {
    LOCAL_MODE,  //对话模式
    LINK_MODE,   //链接模式
};

//机体状态枚举
enum EVA_STATE {
    CHAT_STATE,      //对话模式
    COMPLETE_STATE,  //补完模式
    SERVER_STATE,    //服务模式
};

//设置参数
struct SETTINGS {
    double temp = DEFAULT_TEMP;
    double repeat = DEFAULT_REPEAT;
    int npredict = DEFAULT_NPREDICT;

    int ngl = DEFAULT_NGL;
    int nctx = DEFAULT_NCTX;
    int batch = DEFAULT_BATCH;
    int nthread = std::thread::hardware_concurrency() * 0.5;

    QString modelpath = "";
    QString lorapath = "";
    QString mmprojpath = "";

    bool complete_mode = false;
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
    QString api_endpoint = ""; // openai格式端点 = ip + port
    QString api_ip = "";
    QString api_port = "8080";
    QString api_chat_endpoint = CHAT_ENDPOINT;
    QString api_completion_endpoint = COMPLETION_ENDPOINT;
    bool is_cache = true;
};

// 链接模式下发送消息的对象枚举
enum API_ROLE {
    API_ROLE_USER,
    API_ROLE_ASSISANT,
    API_ROLE_OBSERVATION,
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

//状态区信号枚举
enum SIGNAL_STATE {
    USUAL_SIGNAL,     //一般输出，黑色
    SIGNAL_SIGNAL,    //信号，蓝色
    SUCCESS_SIGNAL,   //成功，绿色
    WRONG_SIGNAL,     //错误，红色
    EVA_SIGNAL,       //机体，紫色
    TOOL_SIGNAL,      //工具，天蓝色
    SYNC_SIGNAL,      //同步，橘黄色
    MATRIX_SIGNAL,    //文本表格，黑色，不过滤回车符
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
    bool is_speech = false;
    QString speech_name = "";
};

//记忆单元（当前记忆）
struct Brain_Cell {
    int id;        //在缓存中的序号
    int token;     //缓存的词索引
    QString word;  //词索引对应的词
};

//同步率测试管理器
struct Syncrate_Manager {
    bool is_sync = false;       // 是否在测试同步率
    bool is_predecode = false;  // 是否预解码过
    QList<int> correct_list;    // 通过回答的题目序号
    float score = 0;            // 每个3.3分，满分99.9，当达到99.9时为最高同步率400%

    // 1-5：计算器使用，6-10：系统终端使用，11-15：知识库使用，16-20：软件控制台使用，21-25：文生图使用，26-30：代码解释器使用
    QList<int> sync_list_index;      // 待完成的回答任务的索引
    QStringList sync_list_question;  // 待完成的回答任务
};

// 文生图参数
#define DEFAULT_SD_NOISE "0.75"   //噪声系数

struct SD_PARAMS {
    QString sample_type; //采样算法euler, euler_a, heun, dpm2, dpm++2s_a, dpm++2m, dpm++2mv2, lcm
    QString negative_prompt;  //反向提示词
    QString modify_prompt;  //修饰词
    int width; //图像宽度
    int height;  //图像高度
    int steps;  //采样步数
    int batch_count; //出图张数
    int seed; //随机数种子 -1随机
    int clip_skip;  //跳层 
    double cfg_scale; //提示词与图像相关系数

    // 构造函数
    SD_PARAMS(
        QString sample_type = "euler", 
        QString negative_prompt = "",  
        QString modify_prompt = "",  
        int width = 512, 
        int height = 512,  
        int steps = 20,  
        int batch_count = 1, 
        int seed = -1, 
        int clip_skip = -1,  
        double cfg_scale = 7.5
    ) : sample_type(sample_type), negative_prompt(negative_prompt), modify_prompt(modify_prompt),
        width(width), height(height), steps(steps), batch_count(batch_count),
        seed(seed), clip_skip(clip_skip), cfg_scale(cfg_scale) {}
};

#endif
