#ifndef XBOT_H
#define XBOT_H
#include <QThread>
#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QColor>
#include <QTextCodec>
#include <QJsonArray>

#include "xconfig.h" //ui和bot都要导入的共有配置
#include "llama.cpp/common/common.h"
#include "llama.cpp/llama.h"
#include "llama.cpp/examples/llava/clip.h"
#include "llama.cpp/examples/llava/llava.h"
#include "llama.cpp/common/stb_image.h"

//llama模型类
class xBot : public QThread
{
    Q_OBJECT
public:
    xBot();
    ~xBot();

    void run() override;//预测和装载,多线程的实现方式
    void load(std::string &modelpath);//装载模型
    void reset(bool is_clear_all);//重置模型上下文和缓存等
    void preDecode();//预解码

    QString viewVocab();//获取模型词表
    QString view_embd(llama_context *ctx_,std::vector<llama_token> embd_);//查看embd
public:
    //拯救中文
    QJsonObject wordsObj;
    int language_flag = 0;
    //实例相关
    llama_model_params hparams;//模型内部参数
    gpt_params gpt_params_;//控制模型的参数,内含控制采样的参数sparams
    llama_sampling_context * sparams;//采样用参数

    llama_model *model;//模型
    llama_context *ctx;//上下文
    clip_ctx * ctx_clip;//clip模型,编码图像用

    //先输出用户发送过来的东西
    //context_pos 0是用户昵称 1是输入内容 2是模型昵称
    void push_out(std::vector<llama_token> embd_output, int context_pos);

    int n_vocab;//词表大小
    int n_ctx_train;//模型最大上下文长度
    int n_gpu_layers;//模型最大gpu负载层数
    int maxngl=0;

    std::vector<llama_token> embd_inp,embd;//待推理词向量
    llama_token eos_token;//结束标志
    std::vector<llama_token_data> *candidates;//词表采样矩阵
    llama_grammar * grammar = NULL; //强制语法
    std::vector<int>   *history_tokens;//记录模型的输出
    std::vector<llama_token> pick_half_utf8;
    int ga_i = 0;//记录拓展的上下文数量?
    int ga_n = 1;//拓展的倍数
    int ga_w = 512;//拓展时用于计算的宽度？group-attention width
    std::vector<llama_token> system_tokens;//系统指令的token

    void apply_date(DATES date);//应用约定

    //计算时间相关
    bool is_batch = false;
    float batch_time = 0.000001;
    float batch_count = 0;
    float singl_time = 0.000001;
    float singl_count = 0;
    int n_remain=-1;
    int stream();//推理循环

    //标签相关
    std::string bot_modelpath = "";//模型路径
    std::string lorapath = "";//lora模型路径
    std::string mmprojpath = "";//mmproj模型路径
    INPUTS input;//用户输入
    bool is_stop=false,is_load =false,is_first_load = true,is_free = false,is_first_reset=false,is_first_input=true;//一些状态控制标签
    bool is_complete = false;//补完模式标签
    bool is_antiprompt = false;//上一次是否有用户昵称,,如果已经检测出用户昵称则不加前缀
    bool add_bos;//是否添加开始标志
    bool is_datetoolong = false;//如果约定的系统指令长度太长则不约定
    int n_past = 0;//已推理个数
    int n_consumed =0;//已编码字符数
    bool is_test = false;//是否正在测试
    int fail = 0;
    bool isIncompleteUTF8(const std::string& text);//检测是否有不完整的utf8字符
    float vfree=0;
    bool is_multi=false;//是否为多模态
    bool is_load_tool=false;//是否挂载了工具
    QStringList extra_stop_words;//额外停止标志
    bool vram_enough = false;

public slots:
    void recv_dateset(DATES ini_DATES,SETTINGS ini_SETTINGS);//自动装载
    void recv_language(int language_flag_);//传递使用的语言
    void recv_imagepath(QString image_path);//接受图片路径
    void recv_input(INPUTS input_,bool is_test_);//接受用户输入
    void recv_stop();//接受停止信号
    void recv_reset(bool is_clear_all);//接受重置信号
    void recv_set(SETTINGS settings,bool can_reload);//接受设置内容
    void recv_date(DATES date);//接受约定内容
    void recv_free();//释放
    void recv_maxngl(int maxngl_);//传递模型最大的ngl值
#ifdef BODY_USE_CUBLAST
    void recv_gpu_status(float vmem,float vram, float vcore, float vfree_);//更新gpu内存使用率
#endif

signals:
    void bot2ui_predecode(QString bot_predecode_);//传递模型预解码内容
    void bot2ui_state(const QString &state_string, STATE state=USUAL_);//发送的状态信号
    void bot2ui_output(const QString &result, bool is_while=1, QColor color=QColor(0,0,0));//发送的输出信号,is_while表示从流式输出的token
    void bot2ui_loadover(bool ok_,float load_time_);//装载完成的信号
    void bot2ui_pushover();//推理完成的信号
    void bot2ui_stopover();//完成停止的信号
    void bot2ui_arrivemaxctx(bool prepush);//模型达到最大上下文的信号
    void bot2ui_resetover();//模型重置完成的信号
    void bot2ui_reload();//gpu负载层数改变,重载模型的信号
    void bot2ui_setreset();//bot发信号请求ui触发reset
    void bot2ui_datereset();//bot发信号请求ui触发reset
    void bot2ui_params(PARAMS p);//bot将模型参数传递给ui
    void bot2ui_vocab(QString model_vocab);//传递模型总词表
    void bot2ui_kv(float percent,int n_past);//传递缓存量
    void bot2ui_tokens(int tokens);//传递测试解码token数量
    void bot2ui_log(QString log);//传递llama.cpp的log
    void bot2ui_play();
};

#endif // XBOT_H
