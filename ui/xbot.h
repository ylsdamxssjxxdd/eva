#ifndef XBOT_H
#define XBOT_H
#include <QColor>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextCodec>
#include <QThread>


#include "llama.cpp/common/common.h"
#include "llama.cpp/common/stb_image.h"
#include "llama.cpp/examples/llava/clip.h"
#include "llama.cpp/examples/llava/llava.h"
#include "llama.cpp/include/llama.h"
#include "xconfig.h"  //ui和bot都要导入的共有配置

// llama模型类
class xBot : public QObject {

    Q_OBJECT

   public:
    xBot();
    ~xBot();

   public slots:
    void load(QString modelpath);  //装载模型
    void reset(bool is_clear_all);      //重置模型上下文和缓存等
    void predict(INPUTS inputs);         //开始预测推理
    void preDecodeSystemPrompt();                   //预解码
    void preDecodeImage(QString image_path);              //预解码图像
    QString viewVocab();                                                     //获取模型词表
    QString view_embd(llama_context *ctx_, std::vector<llama_token> embd_);  //查看embd

   public:
    //拯救中文
    QJsonObject wordsObj;
    int language_flag = 0;
    QString jtr(QString customstr);  // 根据language.json(wordsObj)和language_flag中找到对应的文字
    int get_Chinese_word_nums(QString str_);
    //实例相关
    llama_model_params hparams;       //模型内部参数
    gpt_params gpt_params_;           //控制模型的参数,内含控制采样的参数sparams
    llama_sampling_context *sparams;  //采样用参数

    llama_model *model;  //模型
    llama_context *ctx;  //上下文
    clip_ctx *ctx_clip;  // clip模型上下文, 编码图像用

    // 对话模板相关
    DATES bot_date;// 约定内容
    CHATS bot_chat;// 经过模型自带模板格式化后的内容
    void get_default_templete_chat_format();// 构建并提取系统指令、输入前缀、输入后缀

    // 快捷预解码token
    bool eval_tokens(struct llama_context * ctx_llama, std::vector<llama_token> tokens, int n_batch, int * n_past);
    // 快捷预解码文本
    bool eval_string(struct llama_context * ctx_llama, const char* str, int n_batch, int * n_past, bool add_bos);
    // 预解码图像
    void process_eval_image_embed(llama_context * ctx_llama, clip_ctx * ctx_clip, const struct llava_image_embed * embeds, int n_batch, int * n_past, int idx);
    // 预处理图像
    bool process_image(llama_context * ctx, clip_ctx * ctx_clip, struct llava_image_embed * image_embeds, gpt_params gpt_params_, int &n_past);
    //回调函数,获取llama的日志
    static void bot_log_callback(ggml_log_level level, const char *text, void *user_data);
    //解决半个utf8字符问题
    template <class Iter>
    std::string tokens_to_str(llama_context *ctx, Iter begin, Iter end);
    //转为小写，针对英文字母
    std::string toLowerCaseASCII(const std::string &input);

    //先输出用户发送过来的东西
    // context_pos 0是用户昵称 1是输入内容 2是模型昵称
    void push_out(INPUTS input, std::vector<llama_token> embd_output, int context_pos);

    int n_vocab;       //词表大小
    int n_ctx_train;   //模型最大上下文长度
    int n_gpu_layers;  //模型最大gpu负载层数
    int maxngl = 0;

    std::vector<llama_token> embd_inp, embd;  //待推理词向量
    llama_token eos_token;                    //结束标志，end of sentence
    llama_token eot_token;                    //结束标志，end of turn
    llama_token bos_token;                    //开始标志，begin of sentence
    std::vector<llama_token> spliter_token;   //分隔标志
    llama_grammar *grammar = NULL;            //强制语法
    std::vector<llama_token> pick_half_utf8;
    int ga_i = 0;                            //记录拓展的上下文数量?
    int ga_n = 1;                            //拓展的倍数
    int ga_w = 512;                          //拓展时用于计算的宽度？group-attention width
    std::vector<llama_token> system_tokens;  //系统指令的token

    void apply_date(DATES date);  //应用约定

    //计算时间相关
    bool is_batch = false;
    float batch_time = 0.000001;
    float batch_count = 0;
    float singl_time = 0.000001;
    float singl_count = 0;
    int n_remain = -1;
    int remain_n_remain = 0;
    int stream();  //推理循环

    //标签相关
    std::string bot_modelpath = "";                                                                        //模型路径
    std::string lorapath = "";                                                                             // lora模型路径
    std::string mmprojpath = "";                                                                           // mmproj模型路径                                                                                         //用户输入
    bool is_stop = false, is_load = false, is_first_load = true, is_free = false, is_first_reset = false;  //一些状态控制标签
    bool is_complete = false;                                                                              //补完模式标签
    bool is_antiprompt = false;                                                                            //上一次是否有用户昵称,,如果已经检测出用户昵称则不加前缀
    bool is_datetoolong = false;                                                                           //如果约定的系统指令长度太长则不约定
    int n_past = 0;                                                                                        //已推理个数
    int n_consumed = 0;                                                                                    //已编码字符数
    bool is_test = false;                                                                                  //是否正在测试
    int fail = 0;
    bool isIncompleteUTF8(const std::string &text);  //检测是否有不完整的utf8字符
    float vfree = 0;
    bool is_multi = false;         //是否为多模态
    bool is_load_tool = false;     //是否挂载了工具
    QStringList extra_stop_words;  //额外停止标志
    bool vram_enough = false;
    std::string current_output;            //用来判断里面是否存在反向词
    bool is_debuging = false;              // debug中状态
    int debuging_one = 0;                  // debuging时控制循环只进行一次
    std::vector<Brain_Cell> Brain_vector;  //记忆向量(当前记忆)
    bool showSpecial = true;               // 是否显示特殊标志

   public slots:
    void recv_stop();//接受停止信号
    void recv_llama_log(QString log_);                          //获取llama log
    void recv_debuging(bool is_debuging_);                      //传递debug中状态
    void recv_dateset(DATES ini_DATES, SETTINGS ini_SETTINGS);  //自动装载
    void recv_language(int language_flag_);                     //传递使用的语言
    void recv_reset(bool is_clear_all);                         //接受重置信号
    void recv_set(SETTINGS settings, bool can_reload);          //接受设置内容
    void recv_date(DATES date);                                 //接受约定内容
    void recv_free(bool loadlater);                             //释放
    void recv_gpu_status(float vmem, float vram, float vcore, float vfree_);  //更新gpu内存使用率

   signals:
    void bot2ui_chat_format(CHATS chat); // 发送格式化的对话内容
    void bot2ui_freeover_loadlater();                                   // 模型释放完毕
    void bot2expend_brainvector(std::vector<Brain_Cell> Brain_vector_, int nctx, bool reflash = 0);
    void bot2expend_vocab(QString model_vocab);                                                    //传递模型总词表
    void bot2ui_predecode(QString bot_predecode_);                                                 //传递模型预解码内容
    void bot2ui_state(const QString &state_string, SIGNAL_STATE state = USUAL_SIGNAL);             //发送的状态信号
    void bot2ui_output(const QString &result, bool is_while = 1, QColor color = QColor(0, 0, 0));  //发送的输出信号,is_while表示从流式输出的token
    void bot2ui_loadover(bool ok_, float load_time_);                                              //装载完成的信号
    void bot2ui_pushover();                                                                        //推理完成的信号
    void bot2ui_stopover();                                                                        //完成停止的信号
    void bot2ui_arrivemaxctx(bool prepush);                                                        //模型达到最大上下文的信号
    void bot2ui_resetover();                                                                       //模型重置完成的信号
    void bot2ui_reload();                                                                          // gpu负载层数改变,重载模型的信号
    void bot2ui_setreset();                                                                        // bot发信号请求ui触发reset
    void bot2ui_datereset();                                                                       // bot发信号请求ui触发reset
    void bot2ui_params(MODEL_PARAMS p);                                                            // bot将模型参数传递给ui
    void bot2ui_kv(float percent, int n_past);                                                     //传递缓存量
    void bot2ui_tokens(int tokens);                                                                //传递测试解码token数量
    void bot_llama_log(QString log);                                                               //传递llama.cpp的log
    void bot2ui_play();
};

#endif  // XBOT_H


//-------------------------------------------------------------------------
//----------------------------------多后端支持--------------------------------
//-------------------------------------------------------------------------

// // 定义函数指针类型
// typedef struct llama_model* (*llama_load_model_from_file_t)(const char *path_model, struct llama_model_params params);
// typedef struct llama_context* (*llama_new_context_with_model_t)(struct llama_model * model, struct llama_context_params params);
// typedef bool (*llava_eval_image_embed_t)(struct llama_context * ctx_llama, const struct llava_image_embed * embed, int n_batch, int * n_past);
// typedef struct clip_ctx * (*clip_model_load_t)(const char * fname, int verbosity);

// 定义函数指针
// llama_load_model_from_file_t llama_load_model_from_file_;
// llama_new_context_with_model_t llama_new_context_with_model_;
// llava_eval_image_embed_t llava_eval_image_embed_;
// clip_model_load_t clip_model_load_;

// // 动态加载
// QString accelerate = "cuda";
// QString libraryPath_ggml = applicationDirPath_ + "/llama-dll/ggml-" + accelerate;
// QString libraryPath_llama = applicationDirPath_ + "/llama-dll/llama-" + accelerate;
// QString libraryPath_llava_shared = applicationDirPath_ + "/llama-dll/llava_shared-" + accelerate;

// QLibrary ggml_Lib(libraryPath_ggml);// 加载DLL
// if (!ggml_Lib.load()) {qDebug()<<"Failed to load the DLL." << ggml_Lib.errorString();}

// QLibrary llama_Lib(libraryPath_llama);// 加载DLL
// if (!llama_Lib.load()) {qDebug()<<"Failed to load the DLL." << llama_Lib.errorString();}

// QLibrary llava_shared_Lib(libraryPath_llava_shared);// 加载DLL
// if (!llava_shared_Lib.load()) {qDebug()<<"Failed to load the DLL." << llava_shared_Lib.errorString();}

// // 获取函数指针
// llama_load_model_from_file_ = (llama_load_model_from_file_t)llama_Lib.resolve("llama_load_model_from_file");
// llama_new_context_with_model_ = (llama_new_context_with_model_t)llama_Lib.resolve("llama_new_context_with_model");
// if (!llama_load_model_from_file_) {qDebug()<<"Failed to resolve the function llama_load_model_from_file_.";}
// if (!llama_new_context_with_model_) {qDebug()<<"Failed to resolve the function llama_new_context_with_model_.";}

// llava_eval_image_embed_ = (llava_eval_image_embed_t)llava_shared_Lib.resolve("llava_eval_image_embed");
// if (!llava_eval_image_embed_) {qDebug()<<"Failed to resolve the function llava_eval_image_embed.";}
// clip_model_load_ = (clip_model_load_t)llava_shared_Lib.resolve("clip_model_load");
// if (!clip_model_load_) {qDebug()<<"Failed to resolve the function clip_model_load.";}