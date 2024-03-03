#ifndef XCONFIG_H
#define XCONFIG_H

#include <QDebug>

//约定
#define DEFAULT_PROMPT "You are a helpful assistant."
#define DEFAULT_PREFIX "### User"
#define DEFAULT_SUFFIX "### Assistant"
//采样
#define DEFAULT_NPREDICT 2048
#define DEFAULT_TEMP 0.5
#define DEFAULT_REPEAT 1.2
//推理
#ifdef BODY_USE_32BIT
    #define DEFAULT_NCTX 256
    #define DEFAULT_BATCH 32
#else
    #define DEFAULT_NCTX 2048
    #define DEFAULT_BATCH 256
#endif    

#define DEFAULT_NTHREAD 1
#define DEFAULT_NGL 0
//模型
#define DEFAULT_MODELPATH "D:/soul/qwen-1.8b-q4_0.gguf"


//约定内容
struct DATES{
    QString system_prompt;
    QString input_pfx;
    QString input_sfx;
    bool is_load_tool;

};

//设置参数
struct SETTINGS{
    float temp = DEFAULT_TEMP;
    double repeat = DEFAULT_REPEAT;
    int npredict = DEFAULT_NPREDICT;

    int ngl = DEFAULT_NGL;
    int nctx = DEFAULT_NCTX;
    int batch = DEFAULT_BATCH;
    int nthread = DEFAULT_NTHREAD;

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
    bool is_cache = false;
};

//端点接收参数
struct ENDPOINT_DATA{
    QString date_prompt;
    QString input_pfx;
    QString input_sfx;
    QString input_prompt;//续写模式用
    QStringList user_history;//对话模式用
    QStringList assistant_history;//对话模式用
    bool complete_mode;
    float temp;
    double repeat;
    int n_predict;
    
};

//单参数工具
struct TOOLS{
    QString tool_name;//工具名
    QString func_name;//函数名
    QString func_describe_zh;//功能描述中
    QString func_describe_en;//功能描述英
};

//传递的前缀/输入/后缀
struct INPUTS{
    QString input_prefix;
    QString input;
    QString input_suffix;
};

//状态区信号枚举
enum STATE {
        USUAL_,//一般输出，黑色
        SIGNAL_,//信号，蓝色
        SUCCESS_,//成功，绿色
        WRONG_,//错误，红色
        EVA_,//机体，紫色
        TOOL_,//工具，橘色
};


//extern QMap<QString, DATE_> date_map;//extern定义后可以多个.cpp访问



#endif
