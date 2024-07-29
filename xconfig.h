#ifndef XCONFIG_H
#define XCONFIG_H

#include <QDebug>
#include <QColor>

#include <iostream>
#include <thread>
#include <vector>
#include <array>

//约定内容
struct DATES{
    QString system_prompt;
    QString input_pfx;
    QString input_sfx;
    bool is_load_tool;
    QStringList extra_stop_words;//额外停止标志
};

//默认约定
#define DEFAULT_PROMPT "You are a helpful assistant."
#define DEFAULT_PREFIX "User"
#define DEFAULT_SUFFIX "Assistant"
#define DEFAULT_SPLITER "\n" // 分隔符
#define DEFAULT_THOUGHT "thought: " // 思考词
#define DEFAULT_OBSERVATION "observation: " // 观察词

//发送内容的源
enum ROLE{
    ROLE_USER,// 加前缀后缀输入。                       构成形式：<bos>{{user_name}}{{spliter}}{{user_content}}<eos>{{spliter}}<bos>{{model_name}}{{spliter}}
    ROLE_TOOL,// 不加前缀后缀，用天蓝色输出用户输入部分。 构成形式：{{spliter}}{{observation}}{{tool_content}}{{spliter}}{{thought}}
    ROLE_TEST,// 同时改变is_test标志。                 构成形式：<bos>{{user_name}}{{spliter}}{{user_content}}<bos>{{model_name}}{{spliter}}
    ROLE_DEBUG,// 不加前缀后缀输入。                    构成形式：
    ROLE_THOUGHT,// 后缀末尾的分隔符用 thought: 代替    构成形式：{{spliter}}<bos>{{user_name}}{{spliter}}{{user_content}}<eos>{{spliter}}<bos>{{model_name}}{{spliter}}{{thought}}
};

//传递的前缀/输入/后缀
struct INPUTS{
    QString input_prefix;
    QString input;
    QString input_suffix;
    ROLE role;
};

//采样
#define DEFAULT_NPREDICT 4096
#define DEFAULT_TEMP 0.5
#define DEFAULT_REPEAT 1.2

//推理
#ifdef BODY_USE_32BIT
    #define DEFAULT_NCTX 256
    #define DEFAULT_BATCH 32
#else
    #define DEFAULT_NCTX 4096
    #define DEFAULT_BATCH 2048
#endif    

#define DEFAULT_NTHREAD 1
#define DEFAULT_NGL 0

#define DEFAULT_MODELPATH "D:/soul" // 模型所在文件夹
#define DEFAULT_EMBEDDING_PORT "7758" // 嵌入端口

//默认sd反提示
#define DEFAULT_ANTIPROMPT "EasyNegative,badhandv4,ng_deepnegative_v1_75t,worst quality, low quality, normal quality, lowres, monochrome, grayscale, bad anatomy,DeepNegative, skin spots, acnes, skin blemishes, fat, facing away, looking away, tilted head, lowres, bad anatomy, bad hands, missing fingers, extra digit, fewer digits, bad feet, poorly drawn hands, poorly drawn face, mutation, deformed, extra fingers, extra limbs, extra arms, extra legs, malformed limbs,fused fingers,too many fingers,long neck,cross-eyed,mutated hands,polar lowres,bad body,bad proportions,gross proportions,missing arms,missing legs,extra digit, extra arms, extra leg, extra foot,teethcroppe,signature, watermark, username,blurry,cropped,jpeg artifacts,text,error,Lower body exposure"

//数独题目
#define DEFAULT_SHUDU "\n+---+---+---+---+---+---+\n| 4 |   |   | 2 |   |   |\n+---+---+---+---+---+---+\n|   | 1 |   |   |   | 3 |\n+---+---+---+---+---+---+\n|   |   | 3 |   | 1 |   |\n+---+---+---+---+---+---+\n| 2 |   |   | 3 |   |   |\n+---+---+---+---+---+---+\n|   |   | 1 |   | 4 |   |\n+---+---+---+---+---+---+\n|   | 3 |   |   |   | 5 |\n+---+---+---+---+---+---+"

//llama日志信号字样
#define SERVER_START "model loaded" // server启动成功返回的字样
#define LLM_EMBD "llm_load_print_meta: n_embd           = " // 模型装载成功返回的词嵌入维度字样

//颜色
const QColor SIGNAL_BLUE(0, 0, 255); // 蓝色
const QColor SYSTEM_BLUE(0, 0, 255, 200); // 蓝紫色
const QColor TOOL_BLUE(0, 191, 255); // 天蓝色
const QColor NORMAL_BLACK(0, 0, 0); // 黑色
const QColor LCL_ORANGE(255, 165, 0); // 橘黄色

//不同操作系统相关
#ifdef _WIN32
#define SFX_NAME ".exe" // 第三方程序后缀名
#define OS "windows"
#elif __linux__
#define SFX_NAME "" //第三方程序后缀名
#define OS "linux"
#endif

// 字体
#ifdef _WIN32
#define DEFAULT_FONT "SimSun"
#elif __linux__
#define DEFAULT_FONT "Ubuntu Mono"
#endif
//设置参数
struct SETTINGS{
    double temp = DEFAULT_TEMP;
    double repeat = DEFAULT_REPEAT;
    int npredict = DEFAULT_NPREDICT;

    int ngl = DEFAULT_NGL;
    int nctx = DEFAULT_NCTX;
    int batch = DEFAULT_BATCH;
    int nthread = std::thread::hardware_concurrency()*0.7;

    QString modelpath = "";
    QString lorapath = "";
    QString mmprojpath = "";

    bool complete_mode = false;
};

//模型参数,模型装载后发送给ui的参数
struct PARAMS{
    int n_ctx_train;//最大值
};

//api配置参数
struct APIS{
    QString api_ip = "";
    QString api_port = "8080";
    QString api_chat_endpoint = "/v1/chat/completions";
    QString api_complete_endpoint = "/completion";
    bool is_cache = true;
};

enum API_ROLE{
    API_ROLE_USER,
    API_ROLE_ASSISANT,
    API_ROLE_OBSERVATION,
};

//端点接收参数
struct ENDPOINT_DATA{
    QString date_prompt;
    QString input_pfx;
    QString input_sfx;
    QString input_prompt;//续写模式用
    QVector<QPair<QString, API_ROLE>> insert_history;// 将要构造的历史数据，前面是内容后面是角色
    bool complete_mode;
    float temp;
    double repeat;
    int n_predict;
    QStringList stopwords;//停止标志
};

//单参数工具
struct TOOLS{
    QString tool_name;//工具名
    QString func_name;//函数名
    QString func_describe;//功能描述
};

//状态区信号枚举
enum STATE {
        USUAL_,//一般输出，黑色
        SIGNAL_,//信号，蓝色
        SUCCESS_,//成功，绿色
        WRONG_,//错误，红色
        EVA_,//机体，紫色
        TOOL_,//工具，橘色
        DEBUGING_,//debug，墨绿色
        MATRIX_,//文本表格，黑色，不过滤回车符
};
//模式枚举
enum MODE {
    CHAT_,//对话模式
    COMPLETE_,//补完模式
    SERVER_,//服务模式
    LINK_,//链接模式
};

//whisper可以传入的参数
struct Whisper_Params {
    int32_t n_threads    =  1;
    int32_t n_processors =  1;
    int32_t offset_t_ms  =  0;
    int32_t offset_n     =  0;
    int32_t duration_ms  =  0;
    int32_t progress_step =  5;
    int32_t max_context  = -1;
    int32_t max_len      =  0;
    //int32_t best_of      = whisper_full_default_params(WHISPER_SAMPLING_GREEDY).greedy.best_of;
    //int32_t beam_size    = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH).beam_search.beam_size;
    int32_t audio_ctx   = 0;

    float word_thold    =  0.01f;
    float entropy_thold =  2.40f;
    float logprob_thold = -1.00f;

    bool speed_up        = false;
    bool debug_mode      = false;
    bool translate       = false;
    bool detect_language = false;
    bool diarize         = false;
    bool tinydiarize     = false;
    bool split_on_word   = false;
    bool no_fallback     = false;
    bool output_txt      = false;
    bool output_vtt      = false;
    bool output_srt      = false;
    bool output_wts      = false;
    bool output_csv      = false;
    bool output_jsn      = false;
    bool output_jsn_full = false;
    bool output_lrc      = false;
    bool no_prints       = false;
    bool print_special   = false;
    bool print_colors    = false;
    bool print_progress  = false;
    bool no_timestamps   = false;
    bool log_score       = false;
    bool use_gpu         = true;

    std::string language  = "zh";
    std::string prompt;
    std::string font_path = "/System/Library/Fonts/Supplemental/Courier New Bold.ttf";
    std::string model     = "";

    // [TDRZ] speaker turn string
    std::string tdrz_speaker_turn = " [SPEAKER_TURN]"; // TODO: set from command line

    std::string openvino_encode_device = "CPU";

    std::vector<std::string> fname_inp = {};
    std::vector<std::string> fname_out = {};
};

struct Embedding_Params 
{
    QString modelpath="";
};
// 定义一个结构体来存储索引和值
struct Embedding_vector {
    int index;//用于排序的序号，在表格中的位置
    QString chunk;
    std::array<double, 1024> value; // 最高支持1024维向量
};

//量化方法说明数据结构
struct QuantizeType {
    QString typename_;//方法名
    QString bit;//压缩率,fp16为基准
    QString perplexity; //困惑度
    QString recommand; //推荐度
};

struct SD_Params 
{
    QString runmode = "txt2img";//运行模式txt2img or img2img or convert, default: txt2img
    QString modelpath = "";//模型路径
    QString vaepath = "";//vae路径 sdxl_turbo必须需要D:\\soul\\sd\\sd_xl_vae.safetensors
    QString lora_model_dir = "";//lora所在目录 不指定则在当前目录，使用时在提示词后加<lora:文件名:1> 1是应用强度
    QString sampletype = "euler_a";//采样算法euler, euler_a, heun, dpm2, dpm++2s_a, dpm++2m, dpm++2mv2, lcm
    QString prompt = "";//提示词
    QString negative_prompt = "EasyNegative,badhandv4,ng_deepnegative_v1_75t,worst quality, low quality, normal quality, lowres, monochrome, grayscale, bad anatomy,DeepNegative, skin spots, acnes, skin blemishes, fat, facing away, looking away, tilted head, lowres, bad anatomy, bad hands, missing fingers, extra digit, fewer digits, bad feet, poorly drawn hands, poorly drawn face, mutation, deformed, extra fingers, extra limbs, extra arms, extra legs, malformed limbs,fused fingers,too many fingers,long neck,cross-eyed,mutated hands,polar lowres,bad body,bad proportions,gross proportions,missing arms,missing legs,extra digit, extra arms, extra leg, extra foot,teethcroppe,signature, watermark, username,blurry,cropped,jpeg artifacts,text,error,Lower body exposure";//反向提示词
    QString extra_prompt = "masterpieces, best quality, beauty, detailed, Pixar, 8k, ";//附加提示词
    int batch_count = 1;//出图张数
    int width = 512;//图像宽度
    int height = 512;//图像高度
    int steps = 20;//采样步数
    int seed = -1;//随机数种子 -1随机
    int nthreads = std::thread::hardware_concurrency()*0.7;//线程数
    float cfg_scale = 7.5;//提示词与图像相关系数
    float noise_strength = 0.75;//噪声系数
    float clip_skip = 2;//跳层
    QString outpath = "";//输出路径

};

//文转声参数
struct Voice_Params 
{
    bool is_voice = false;
    QString voice_name = "";
};

//记忆单元（当前记忆）
struct Brain_Cell
{
    int id;//在缓存中的序号
    int token;//缓存的词索引
    QString word;//词索引对应的词
};

#endif
