#ifndef EXPEND_H
#define EXPEND_H
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <QAbstractSocket>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMediaPlayer>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QPlainTextEdit>
#include <QProcess>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QTextCodec>
#include <QTextStream>
#include <QTextToSpeech>
#include <QTimer>
#include <QWidget>
#include <algorithm>
#include <math.h>
#ifdef _WIN32
#elif __linux__
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#endif

#include "../mcp_tools.h"
#include "../utils/vectordb.h"
#include "../xconfig.h"
#include "./src/utils/toggleswitch.h"
namespace Ui
{
class Expend;
}

class Expend : public QWidget
{
    Q_OBJECT
    //-------------------------------------------------------------------------
    //----------------------------------界面相关--------------------------------
    //-------------------------------------------------------------------------
  public:
    Expend(QWidget *parent = nullptr, QString applicationDirPath_ = "./");
    ~Expend();
    QString applicationDirPath;
    void closeEvent(QCloseEvent *event) override; // 关闭事件
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override; // 事件过滤器函数
    void setWhisperModelpath(QString modelpath);            // 用于设置whisper模型路径
    void setSdModelpath(QString modelpath);                 // 用于设置sd模型路径
    QJsonObject wordsObj;
    int language_flag = 0;
    QString jtr(QString customstr); // 根据language.json(wordsObj)和language_flag中找到对应的文字
    bool is_first_show_modelproliferation = true;
    bool is_first_show_sync = true;
    bool is_first_show_expend = true;
    bool is_first_show_logs = true;
    bool is_first_show_info = true;
    bool load_percent_tag = false;
    void init_expend(); // 初始化增殖窗口
    bool copyFile(const QString &src, const QString &dst);
    bool createTempDirectory(const QString &path);
    bool copyRecursively(const QString &srcFilePath, const QString &tgtFilePath); // 复制一个目录里的文件夹所有东西到另一个文件夹
    QString customOpenfile(QString dirpath, QString describe, QString format);
    void readConfig(); // 读取配置文件并应用
    QString currentpath;
    QString convertmodeldir;
    void showReadme();                      // 展示readme内容
    bool removeDir(const QString &dirName); // 删除文件夹

  signals:
    void expend2ui_state(QString state_string, SIGNAL_STATE state);

  public slots:
    void recv_language(int language_flag_);      // 传递语言标志
    void recv_expend_show(EXPEND_WINDOW window); // 通知显示增殖窗口
    void recv_llama_log(QString log);            // 传递llama.cpp的log
    // Eval: receive current UI mode/apis/settings snapshot from main UI
    void recv_eval_mode(EVA_MODE m) { eval_mode = m; }
    void recv_eval_apis(APIS a)
    {
        eval_apis = a;
        updateEvalInfoUi();
    }
    void recv_eval_settings(SETTINGS s)
    {
        eval_settings = s;
        updateEvalInfoUi();
    }
  private slots:
    void on_tabWidget_tabBarClicked(int index); // 用户切换选项卡时响应
    void onTabCurrentChanged(int index);        // 选项卡变更（包含编程切换）
    void updateModelInfoAnim();                 // 根据可见性/选项卡启停动画（模型信息/软件介绍）
    // Eval: user actions
    void on_eval_start_pushButton_clicked();
    void on_eval_stop_pushButton_clicked();

  private:
    Ui::Expend *ui;
    //-------------------------------------------------------------------------
    //----------------------------------声转文相关--------------------------------
    //-------------------------------------------------------------------------
  public:
    QProcess *whisper_process;     // 用来启动whisper
    Whisper_Params whisper_params; // whisper可以传入的参数
    int max_thread;
    QElapsedTimer whisper_time; // 计时器
    QString wavpath;
    bool is_handle_whisper = false; // 是否为手动转换
  signals:
    void whisper_kill();
    void expend2ui_speechdecode_over(QString result);
    void expend2ui_whisper_modelpath(QString modelpath);
  public slots:
    void recv_speechdecode(QString wavpath, QString out_format = "txt"); // 开始语音转文字
    void whisper_onProcessStarted();
    void whisper_onProcessFinished();
  private slots:
    void on_whisper_load_modelpath_button_clicked(); // 用户点击选择whisper路径时响应
    void on_whisper_wavpath_pushButton_clicked();    // 用户点击选择wav路径时响应
    void on_whisper_execute_pushbutton_clicked();    // 用户点击执行转换时响应

    //-------------------------------------------------------------------------
    //----------------------------------知识库相关--------------------------------
    //-------------------------------------------------------------------------
  public:
    QStringList tokenizeContent(const QString &content); // 分词函数
    Embedding_Params embedding_params;
    int embedding_resultnumb = 3;       // 嵌入结果返回个数
    bool embedding_server_need = false; // 下一次打开是否需要自启动嵌入服务
    bool embedding_embed_need = false;  // 下一次打开是否需要自动构建知识库
    bool keep_embedding_server = false; // 确保嵌入服务不会因为刚启动就停止
    QString embedding_server_api = "http://" + QString(DEFAULT_EMBEDDING_IP) + ":" + DEFAULT_EMBEDDING_PORT + DEFAULT_EMBEDDING_API;
    QProcess *server_process;
    // Cache llama-server PID to ensure we can terminate even if QProcess loses tracking
    qint64 embedding_server_pid = -1;
    // 停止知识库嵌入服务；force=true 时强制杀进程树（Windows 使用 taskkill /T /F）
    void stopEmbeddingServer(bool force = false);
    void embedding_server_start();                  // 尝试启动server
    QString txtpath;                                // 用户上传的txt文件路径
    QStringList upload_paths;                       // selected source files (txt/md/docx)
    int embedding_server_dim = 1024;                // 开启嵌入服务的嵌入维度
    void preprocessTXT();                           // 预处理文件内容
    void preprocessFiles(const QStringList &paths); // preprocess multiple files
    int show_chunk_index = 0;                       // 待显示的嵌入文本段的序号
    QVector<Embedding_vector> Embedding_DB;         // 嵌入的所有文本段的词向量，向量数据库
    VectorDB vectorDb;                              // SQLite 持久化向量库
    Embedding_vector user_embedding_vector;
    double cosine_similarity(const std::vector<double> &a, const std::vector<double> &b);
    std::vector<std::pair<int, double>> similar_indices(const std::vector<double> &user_vector, const QVector<Embedding_vector> &embedding_DB);

  signals:
    void expend2tool_embeddingdb(QVector<Embedding_vector> Embedding_DB_); // 发送已嵌入文本段数据给tool
    void expend2ui_embeddingdb_describe(QString describe);                 // 传递知识库的描述
    void expend2ui_embedding_resultnumb(int resultnumb);                   // 传递嵌入结果返回个数
  public slots:
    void embedding_processing(); // 知识库构建过程
    void readyRead_server_process_StandardOutput();
    void readyRead_server_process_StandardError();
    void server_onProcessStarted();  // 进程开始响应
    void server_onProcessFinished(); // 进程结束响应
  private slots:
    void show_embedding_txt_wait_menu(const QPoint &pos);         // 右击表格显示菜单
    void embedding_txt_wait_onAdd();                              // 添加表格
    void embedding_txt_wait_onDelete();                           // 删除表格
    void show_embedding_txt_over_menu(const QPoint &pos);         // 右击已嵌入表格显示菜单
    void embedding_txt_over_onDelete();                           // 删除已嵌入选中行（多选）并重排索引
    void on_embedding_txt_modelpath_button_clicked();             // 用户点击选择嵌入模型路径时响应
    void on_embedding_txt_upload_clicked();                       // 用户点击上传文档时响应
    void on_embedding_txt_embedding_clicked();                    // 用户点击嵌入时响应
    void on_embedding_test_pushButton_clicked();                  // 用户点击检索时响应
    void on_embedding_model_lineedit_textChanged();               // 嵌入端点改变响应
    void on_embedding_txt_describe_lineEdit_textChanged();        // 知识库描述改变响应
    void on_embedding_resultnumb_spinBox_valueChanged(int value); // 嵌入结果返回个数改变响应

    //-------------------------------------------------------------------------
    //----------------------------------模型量化相关--------------------------------
    //-------------------------------------------------------------------------
  public:
    QString shell = DEFAULT_SHELL;
    QString pythonExecutable = DEFAULT_PYTHON;
    QVector<QuantizeType> quantize_types; // 量化方法说明数据
    void show_quantize_types();           // 展示量化方法
    void output_modelpath_change();       // 根据待量化模型路径和量化方法填入量化后模型路径
    QProcess *quantize_process;
    void quantize(QString in_modelpath, QString out_modelpath, QString important_datapath, QString quantize_type);
  private slots:
    void on_model_quantize_row_modelpath_pushButton_clicked();      // 用户点击选择待量化模型路径时响应
    void on_model_quantize_important_datapath_pushButton_clicked(); // 用户点击选择重要性矩阵路径时响应
    void on_model_quantize_row_modelpath_lineedit_textChanged();    // 待量化模型路径改变响应
    void on_model_quantize_type_currentIndexChanged(int index);     // 量化方法改变响应
    void on_model_quantize_execute_clicked();                       // 用户点击执行量化按钮时响应
    void quantize_onProcessStarted();                               // 开始信号
    void quantize_onProcessFinished();                              // 结束信号

    //-------------------------------------------------------------------------
    //----------------------------------文生图相关--------------------------------
    //-------------------------------------------------------------------------

  public:
    QString sd_current_template = "default";      // 当前使用的模板 default,sd1.5-anything-3,sdxl-animagine-3.1,sd3-medium,flux1-dev,custom1,custom2
    QMap<QString, SD_PARAMS> sd_params_templates; // sd参数模板们
    void sd_apply_template(SD_PARAMS sd_params);  // 应用sd参数模板
    QString sd_outputpath;                        // 最终的绘制结果保存路径
    QProcess *sd_process;
    bool is_handle_sd = true;
    QString sd_process_output;
    bool img2img = false;                         // 是否是图生图操作
    QStringList listFiles(const QString &path);   // 遍历目录
    bool is_readconfig = false;                   // 用于控制模板的应用
    bool is_sd_custom1 = false;                   // 当前是否为自定义的参数模板
    bool is_sd_custom2 = false;                   // 当前是否为自定义的参数模板
    void sd_save_template(QString template_name); // 保存参数到自定义模板
  public slots:
    void sd_onProcessStarted();  // 进程开始响应
    void sd_onProcessFinished(); // 进程结束响应

    void recv_draw(QString prompt_); // 接收到tool的开始绘制图像信号
  signals:
    void expend2tool_drawover(QString result_, bool ok_); // 绘制完成信号
  private slots:
    void on_sd_modelpath_pushButton_clicked();                       // 用户点击选择sd模型路径时响应
    void on_sd_vaepath_pushButton_clicked();                         // 用户点击选择vae模型路径时响应
    void on_sd_clip_l_path_pushButton_clicked();                     // 用户点击选择clip模型路径时响应
    void on_sd_clip_g_path_pushButton_clicked();                     // 用户点击选择clip模型路径时响应
    void on_sd_t5path_pushButton_clicked();                          // 用户点击选择t5模型路径时响应
    void on_sd_lorapath_pushButton_clicked();                        // 用户点击选择lora模型路径时响应
    void on_sd_draw_pushButton_clicked();                            // 用户点击文生图时响应
    void on_sd_img2img_pushButton_clicked();                         // 用户点击图生图时响应
    void on_params_template_comboBox_currentIndexChanged(int index); // 参数模板改变响应

    //-------------------------------------------------------------------------
    //----------------------------------文转声相关--------------------------------
    //-------------------------------------------------------------------------

  public:
    QString outettsDir;                                     // outetts生成的音频存放目录
    QString outetts_last_output_file;                       // 最近一次 outetts 指定的输出文件
    QStringList avaliable_speech_list;                      // 可用声源列表
    void set_sys_speech(QStringList avaliable_speech_list); // 设置系统可用声源
    Speech_Params speech_params;
    QTextToSpeech *sys_speech;
    QMediaPlayer *speech_player;
    bool is_sys_speech_available = false; // 语音朗读是否可用
    bool is_speech = false;               // 是否系统声源正在朗读
    bool is_speech_play = false;          // 是否音频正在播放
    QTimer speechTimer;                   // 朗读定时器,每秒检查列表，列表中有文字就读然后删，直到读完
    QTimer speechPlayTimer;               // 用来控制播放outetts产生的音频，和wait_speech_play_list搭配使用
    QStringList wait_speech_txt_list;     // 等待的文本列表, 重置停止时清空, 每读一段删除一段, 遇到叹号/分号/顿号/逗号/句号/回车/冒号/进行分段
    QStringList wait_speech_play_list;    // 等待播放的音频列表，存储的是路径
    QString temp_speech_txt;
    void outettsProcess(QString str); // 使用outetts进行文转声
    QProcess *outetts_process;        // 用来启动llama-tts

  signals:
    void expend2ui_speechover();
  public slots:
    void speech_player_over(QMediaPlayer::MediaStatus status); // 音频播放完响应
    void start_tts(QString str);                               // 开始文字转语音
    void speechOver();
    void speechPlayOver();
    void recv_output(const QString result, bool is_while, QColor color); // 接收模型的输出
    void onNetTurnDone();                                       // 一轮推理结束：冲刷缓存片段以便朗读
    void recv_resettts();                                                // 重置文字转语音
    void speech_process();                                               // 每半秒检查列表，列表中有文字就读然后删，直到读完
    void speech_play_process();                                          // 每半秒检查播放列表，列表中有文字就读然后删，直到读完
    void readyRead_outetts_process_StandardOutput();
    void readyRead_outetts_process_StandardError();
    void outetts_onProcessStarted();  // 进程开始响应
    void outetts_onProcessFinished(); // 进程结束响应
    void startNextTTSIfIdle();        // 若空闲则立即开始下一段生成
    void startNextPlayIfIdle();       // 若空闲则立即开始下一段播放
  private slots:
    void speech_enable_change(); // 用户点击启用声音选项响应
    void speech_source_change(); // 用户切换音源响应
    void on_speech_outetts_modelpath_pushButton_clicked();
    void on_speech_wavtokenizer_modelpath_pushButton_clicked();
    void on_speech_manual_pushButton_clicked();

    //-------------------------------------------------------------------------
    //----------------------------------模型信息相关--------------------------------
    //-------------------------------------------------------------------------

  public:
    QString vocab;
    bool is_first_show_modelinfo = true;
    QString model_logs;

    //-------------------------------------------------------------------------
    //----------------------------------模型评估相关--------------------------------
    //-------------------------------------------------------------------------
  private:
    // Snapshot from main UI
    EVA_MODE eval_mode = LOCAL_MODE;
    APIS eval_apis;         // endpoint/key/model
    SETTINGS eval_settings; // sampling/back-end
    // Internal evaluator state
    class xNet *evalNet = nullptr; // dedicated network worker for eval
    QThread *evalThread = nullptr; // worker thread
    bool evalRunning = false;
    bool evalFirstToken = false;
    QElapsedTimer evalTimer; // general timer
    QString evalAccum;       // aggregated (visible+think stripped) content of current turn
    // Debug buffers for eval turns (used notably in QA to inspect reasoning)
    QString evalAccumRaw_;       // raw streamed content including <think> markers
    QString evalReasoning_;      // aggregated content inside <think>..</think>
    QString evalAnswer_;         // aggregated content outside <think>..</think>
    bool evalThinkMode_ = false; // whether the current stream cursor is inside <think>
    // Metrics
    double m_firstTokenMs = -1.0;
    double m_promptTokPerSec = -1.0;
    double m_genTokPerSec = -1.0;
    double m_genCharsPerSec = -1.0;
    // Stream-chunk counting for language-agnostic token speed estimation
    int m_genStreamChunks = 0; // increments on each non-empty streamed chunk during gen-speed stage
    double m_qaScore = -1.0;
    double m_logicScore = -1.0; // logical reasoning score (0..100)
    double m_toolScore = -1.0;
    double m_syncRate = -1.0; // overall weighted score (0..100)
    // Steps progress
    int stepsUnitsTotal = 5; // latency(1)+gen(1)+QA(N)+logic(N)+tool(M), recomputed at start
    int stepsDone = 0;
    QElapsedTimer stepTimer;
    // QA set state
    QVector<QPair<QString, QString>> qaPairs_;
    int qaIndex_ = 0;
    int qaCorrect_ = 0;
    int qaPlanned_ = 5; // planned QA question count for progress planning
    // Logical reasoning set state
    QVector<QPair<QString, QString>> logicPairs_;
    int logicIndex_ = 0;
    int logicCorrect_ = 0;
    int logicPlanned_ = 5;
    // Tools test cases
    struct ToolCase
    {
        QString name;
        QString user;
        QString desc;
    };
    QVector<ToolCase> toolCases_;
    int toolIndex_ = 0;
    int toolCorrect_ = 0;
    // Generation test measurement: exclude think time
    bool genCounting_ = false;
    qint64 genStartNsRel_ = 0; // relative to stepTimer
    // Generation speed multi-run (average over N runs)
    int genPlanned_ = 2;           // number of generation speed runs
    int genRunIndex_ = 0;          // current finished runs count
    double genTokPerSecSum_ = 0.0; // sum of per-run tok/s for averaging
    // Eval pipeline
    int evalStep = 0; // 0..N
    void ensureEvalNet();
    void updateEvalInfoUi();
    void evalResetUi();
    void evalLog(const QString &line);
    void evalSetTable(int row, const QString &name, const QString &val, const QString &desc = QString());
    void evalSetStatus(int row, const QString &status);
    void evalSetElapsed(int row, double seconds);
    void evalNext();
    void evalInitTable();
    void evalUpdateProgress();
    void updateScoreBars();
    void setValueColor(int row, const QString &nameKey, double scoreOrValue, const QString &metric);
    // Sub-tests
    void runLatencyTest();
    void runGenSpeedTest();
    void runQATest();
    void runLogicTest();
    void runToolcallTest();
    void evalFinish();
    // Helpers
    ENDPOINT_DATA makeBaseData(double temp = 0.2, int npredict = 64);
    QJsonArray makeMsgs(const QString &sys, const QString &user);
    QChar parseMCAnswer(const QString &ans);
    // Shutdown helper: gracefully stop eval worker thread and xNet
    void shutdownEvalWorker();
  private slots:
    // evalNet handlers
    void onEvalOutput(const QString &text, bool streaming, QColor color);
    void onEvalState(const QString &line, SIGNAL_STATE st);
    void onEvalSpeeds(double prompt_per_s, double gen_per_s);
    void onEvalPushover();

    //-------------------------------------------------------------------------
    //--------------------------------mcp服务器相关-----------------------------
    //-------------------------------------------------------------------------

  public:
    McpToolManager toolManager; // mcp工具管理器
  signals:
    void expend2mcp_addService(QString mcp_json_str);

  public slots:
    void recv_mcp_message(QString message);
    void recv_addService_single_over(QString name, MCP_CONNECT_STATE state); // 添加某个mcp服务完成
    void recv_addService_over(MCP_CONNECT_STATE state);
    void on_mcp_server_reflash_pushButton_clicked();
    void on_mcp_server_help_pushButton_clicked();
    void add_mcp_server_iteration(QString name, MCP_CONNECT_STATE state); // 添加mcp服务信息
  public:
    bool is_first_show_modelcard = true;
    // TTS streaming parser state
    bool tts_in_think_ = false; // skip content inside <think>..</think>
};
#endif // EXPEND_H
