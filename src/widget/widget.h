#ifndef WIDGET_H
#define WIDGET_H

#include <QApplication>
#include <QAudioEncoderSettings>
#include <QAudioInput>
#include <QAudioRecorder>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHostInfo>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMediaPlayer>
#include <QMenu>
#include <QNetworkAccessManager>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressBar>
#include <QRadioButton>
#include <QResource>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSlider>
#include <QSystemTrayIcon>
#include <QTcpSocket>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCodec>
#include <QTextCursor>
#include <QTextEdit>
#include <QThread>
#include <QTimer>
#include <QWidget>
#include <QtGlobal>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#elif __linux__

#endif

#include "ui_date_dialog.h"
#include "ui_settings_dialog.h"
// #include "utils/globalshortcut.h"
#include "../prompt.h"
#include "../utils/customswitchbutton.h"
#include "../utils/cutscreendialog.h"
#include "../utils/doubleqprogressbar.h"
#include "../xconfig.h" // ui和bot都要导入的共有配置
#include "../xbackend.h" // local llama.cpp server manager
#include "../utils/history_store.h" // per-session history persistence
#include "thirdparty/QHotkey/QHotkey/qhotkey.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
class Widget;
}
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

  public:
    Widget(QWidget *parent = nullptr, QString applicationDirPath_ = "./");
    ~Widget();
    QString applicationDirPath;
    bool eventFilter(QObject *obj, QEvent *event) override; // 事件过滤器函数
    // QShortcut *shortcutF1, *shortcutF2, *shortcutCtrlEnter;
    QHotkey *shortcutF1, *shortcutF2, *shortcutCtrlEnter;
    bool checkAudio();                            // 检测音频支持
    void changeEvent(QEvent *event) override;     // 处理窗口状态变化
    void closeEvent(QCloseEvent *event) override; //关闭事件
  public:
    QJsonObject wordsObj;                  //中文英文
    void getWords(QString json_file_path); //中文英文
    int language_flag = 0;                 // 0是中文1是英文
    QString jtr(QString customstr);        // 根据language.json(wordsObj)和language_flag中找到对应的文字

    // ui相关
    QString ui_output, ui_state_info;
    void output_scroll(QString output, QColor color = QColor(0, 0, 0)); //向output末尾添加文本并滚动
    bool is_stop_output_scroll = false;                                 //输出区滚动标签
    QMenu *right_menu;                                                  //输入区右击菜单
    QScrollBar *output_scrollBar;                                       //输出区滑动条
    bool createTempDirectory(const QString &path);                      //创建临时文件夹
    void create_right_menu();                                           //添加右击问题
    void create_tray_right_menu();                                      //添加托盘右击事件
    mcp::json tools_call;                                               //提取出来的工具名和参数
    QString customOpenfile(QString dirpath, QString describe, QString format);
    QFont ui_font; //约定和设置的字体大小
    QMediaPlayer music_player;
    QString currentpath;
    QString historypath;
    void apply_language(int language_flag_); //改变语种相关
    QSystemTrayIcon *trayIcon;               // 系统托盘图标
    QMenu *trayMenu;                         // 托盘右键菜单
    QIcon EVA_icon;                          // 机体的图标
    QString EVA_title;                       // 机体的标题

    bool is_config = false; //是否是读取了配置进行的装载
    void auto_save_user();  //每次约定和设置后都保存配置到本地
    void get_set();         //获取设置中的纸面值
    void get_date();        //获取约定中的纸面值

    void ui_state_init();      //初始界面状态
    void ui_state_loading();   //装载中界面状态
    void ui_state_pushing();   //推理中界面状态
    void ui_state_servering(); //服务中界面状态
    void ui_state_normal();    //待机界面状态
    void ui_state_recoding();  //录音界面状态

    //模型控制相关
    EVA_CHATS_TEMPLATE bot_chat;       // 经过模型自带模板格式化后的内容
    QMap<QString, EVA_DATES> date_map; //约定模板
    QString custom1_date_system;
    QString custom1_user_name;
    QString custom1_model_name; //自定义约定模板1
    QString custom2_date_system;
    QString custom2_user_name;
    QString custom2_model_name;      //自定义约定模板2
    void preLoad();                  //装载前动作
    bool is_load = false;            //模型装载标签
    bool is_load_play_over = false;  //模型装载动画结束后
    bool is_run = false;             //模型运行标签,方便设置界面的状态
    EVA_MODE ui_mode = LOCAL_MODE;   //机体的模式
    EVA_STATE ui_state = CHAT_STATE; //机体的状态

    QString history_lorapath = "";
    QString history_mmprojpath = "";
    QString ui_template = "default";    //模板
    QString bot_predecode_content = ""; //模型预解码的内容
    void normal_finish_pushover();      //正常情况处理推理完毕
    bool gpu_wait_load = false;         // 等待检测完显存信息重新装载的标签

    EVA_DATES ui_DATES;   // ui的约定
    SETTINGS ui_SETTINGS; // ui的设置
    int kvTokensLast_ = 0;            // last known used tokens for kv cache (accumulate within one conversation)
    int ui_n_ctx_train = 2048; //模型最大上下文长度
    int kvTokensAccum_ = 0;        // accumulated used tokens across the conversation
    int kvTokensTurn_  = 0;        // this-turn processed tokens (prompt_n + generated)
    int server_nctx_ = 0;                 // captured from llama_server logs for verification
    int ui_maxngl = 0;         //模型可卸载到gpu上的层数
    bool load_percent_tag;
    int max_thread = 1; //最大线程数
    int lastReasoningTokens_ = 0; // approximate reasoning tokens in last turn

    float load_time = 0;
    QElapsedTimer load_timer;            // measure local-server load duration
    QTimer *force_unlockload_pTimer; //到时间强制解锁

    // 监视相关
    bool is_monitor = false;
    double ui_monitor_frame = 0; // 监视帧率 多少帧/秒
    QString saveScreen();        //保存屏幕截图
    QTimer monitor_timer;        // 监视定时器 1000/ui_monitor_frame

    //扩展相关
    QString embeddingdb_describe; //知识库的描述

    //视觉相关
    CutScreenDialog *cutscreen_dialog;

    //服务相关（服务模式已移除；本地使用 LocalServerManager 自动启动 llama-server）
    LocalServerManager *serverManager = nullptr; // new: manages local llama.cpp server
    QString ui_port = "8080";
    QString ipAddress = "";
    QString getFirstNonLoopbackIPv4Address(); //获取本机第一个ip地址
    bool lastServerRestart_ = false; // 标记最近一次 ensureLocalServer 是否触发了重启

    //语音相关
    QAudioRecorder audioRecorder;
    QAudioEncoderSettings audioSettings;
    void recordAudio();         //开始录音
    bool is_recodering = false; //是否正在录音
    int audio_time = 0;
    QString outFilePath;
    QTimer *audio_timer;
    QString whisper_model_path = "";

    //设置按钮相关
    void set_SetDialog(); //设置设置选项
    Ui::Settings_Dialog_Ui *settings_ui;
    QDialog *settings_dialog;
    void chooseLorapath();
    void chooseMmprojpath();

    //约定选项相关
    QString shell = DEFAULT_SHELL;
    QString pythonExecutable = DEFAULT_PYTHON;
    void set_DateDialog(); //设置约定选项
    Ui::Date_Dialog_Ui *date_ui;
    QDialog *date_dialog;
    QString ui_extra_lan = "zh";
    QString ui_extra_prompt;
    QString ui_date_prompt;
    bool ui_calculator_ischecked = false;
    bool ui_knowledge_ischecked = false;
    bool ui_stablediffusion_ischecked = false;
    bool ui_controller_ischecked = false;
    bool ui_MCPtools_ischecked = false;
    bool ui_engineer_ischecked = false;
    QString create_extra_prompt();  //构建附加指令
    QString create_engineer_info(); //构建工程师指令
    void tool_change();             //响应工具选择
    void change_api_dialog(bool enable);
    QString checkPython();  //获取环境中的python版本以及库信息
    QString checkCompile(); //获取环境中的编译器版本
    QString python_env = "";
    QString compile_env = "";
    QString truncateString(const QString &str, int maxLength);

    //工具相关
    void addStopwords();               //添加额外停止标志
    bool is_load_tool = false;         //是否挂载了工具
    mcp::json XMLparser(QString text); //手搓输出解析器，提取XMLparser
    QString tool_result;
    QStringList wait_to_show_images_filepath; //文生图后待显示图像的图像路径
    QString screen_info;
    QString create_screen_info(); //构建屏幕信息

    //装载动画相关
    int all_fps = 142;    //总帧数
    int load_percent;     //装载百分比
    void load_log_play(); //按日志显示装载进度
    QVector<QString> movie_line;
    QVector<QPointF> movie_dot;
    QVector<QColor> movie_color;
    QTextCharFormat movie_format; //动画内容格式
    QFont movie_font;             //动画内容字体
    int load_action = 0;          //动作计数
    QTimer *load_pTimer, *load_over_pTimer, *load_begin_pTimer;
    void init_movie();                                      //初始化动画参数
    void load_move();                                       //下一帧动画
    void load_play();                                       //连续播放
    void set_dotcolor(QTextCharFormat *format, int action); //设置点颜色
    int playlineNumber = 0;                                 //动画播放的起始行

    //解码码动画相关
    QTimer *decode_pTimer;
    int decode_action = 0;      //动作计数
    int currnet_LineNumber = 0; //上一次解码动画所在行
    bool is_decode = false;     //解码中标签
    void decode_move();         //下一帧
    void decode_play();         //播放解码中动画
    // 优雅等待动画：记录起始行与用时
    int decodeLineNumber_ = -1;     // 动画所在行（固定行）
    QElapsedTimer decodeTimer_;     // 动画计时器（秒）

    //系统信息相关
    QString model_memusage = "0", ctx_memusage = "0";
    QString model_vramusage = "0", ctx_vramusage = "0";
    float vfree = 0;              //可用显存大小
    int modelsize_MB = 0;         //模型占用空间大小
    bool is_first_getvram = true; //%
    bool is_first_getmem = true;  //%
    float first_vramp = 0;        //%
    float first_memp = 0;         //%

    //链接模式相关，EVA_MODE为LINK_时的行为
    void setApiDialog();                               //初始化设置api选项
    void set_api();                                    //应用api设置
    void startConnection(const QString &ip, int port); //检测ip是否通畅
    void api_send_clicked_slove();                     //链接模式的发送处理
    QDialog *api_dialog;
    QLabel *api_endpoint_label;
    QLineEdit *api_endpoint_LineEdit;
    QLabel *api_key_label;
    QLineEdit *api_key_LineEdit;
    QLabel *api_model_label;
    QLineEdit *api_model_LineEdit;
    APIS apis;                           // api配置参数
    QJsonArray ui_messagesArray;         // 将要构造的历史数据
    QString temp_assistant_history = ""; //临时数据
    QString current_api;                 //当前负载端点
    int currentSlotId_ = -1;             // llama-server slot id for this conversation
    HistoryStore *history_ = nullptr;    // persistent history writer

    //发给模型的信号
  signals:
    // 将后端（llama-server）日志输出给增殖窗口的“模型日志”
    void ui2expend_llamalog(QString log);
    void ui2bot_dateset(EVA_DATES ini_DATES, SETTINGS ini_SETTINGS); //自动装载
    void ui2bot_language(int language_flag_);                        //传递使用的语言
    void ui2bot_predict(EVA_INPUTS input);                           //开始推理
    void ui2bot_stop();                                              //传递推理停止信号
    void ui2bot_reset();                                             //传递重置信号
    void ui2bot_date(EVA_DATES date);                                //传递约定内容
    void ui2bot_set(SETTINGS settings, bool can_reload);             //传递设置内容
    void ui2bot_free(bool loadlater);                                //释放
    void ui2bot_maxngl(int maxngl_);
    void ui2bot_preDecode();                        //从补完模式回来强行预解码
    void ui2bot_monitor_filepath(QString filePath); //给模型发信号，能处理就处理

    //发给net的信号
  signals:
    void ui2net_language(int language_flag_); //传递使用的语言
    void ui2net_push();                       //开始推理
    void ui2net_data(ENDPOINT_DATA data);     //传递端点参数
    void ui2net_apis(APIS apis);              //传递api设置参数
    void ui2net_stop(bool stop);              //传递停止信号

    //发送给tool的信号
    void ui2tool_language(int language_flag_); //传递使用的语言
    void ui2tool_exec(mcp::json tools_call);   //开始推理
    void recv_controller_over(QString result);

    //发送给expend的信号
    void ui2expend_language(int language_flag_);                      //传递使用的语言
    void ui2expend_show(EXPEND_WINDOW window);                        //通知显示扩展窗口
    void ui2expend_speechdecode(QString wavpath, QString out_format); //传一个wav文件开始解码
    void ui2expend_resettts();                                        //重置文字转语音

    //自用信号
  signals:
    void gpu_reflash(); //强制刷新gpu信息
    void cpu_reflash(); //强制刷新gpu信息

    //处理模型信号的槽
  public slots:
    // Ensure local server exists for LOCAL_MODE and wire API endpoint
    void ensureLocalServer();
    void onServerReady(const QString &endpoint);
    void onServerOutput(const QString &line);                 // parse llama_server logs for n_ctx
    void recv_predecoding();                                                     // 正在预解码
    void recv_predecoding_over();                                                // 完成预解码
    void recv_chat_format(EVA_CHATS_TEMPLATE chats);                             //传递格式化后的对话内容
    void recv_freeover_loadlater();                                              //模型释放完毕并重新装载
    void recv_predecode(QString bot_predecode_content_);                         //传递模型预解码的内容
    void recv_toolpushover(QString tool_result_);                                //处理tool推理完毕的槽
    void reflash_output(const QString result, bool is_while, QColor color);      //更新输出区,is_while表示从流式输出的token
    void reflash_state(QString state_string, SIGNAL_STATE state = USUAL_SIGNAL); //更新状态区
    void recv_pushover();                                                        //推理完毕的后处理
    void recv_stopover();                                                        //停止完毕的后处理
    void recv_arrivemaxctx(bool prepush);                                        //模型达到最大上下文的后处理
    void recv_resetover();                                                       //重置完毕的后处理
    void recv_reload();                                                          // gpu负载层数改变,重载模型
    void recv_setreset();                                                        // bot发信号请求ui触发reset
    void recv_datereset();                                                       // bot发信号请求ui触发reset
    void recv_params(MODEL_PARAMS p);                                            // bot将模型参数传递给ui
    void recv_kv(float percent, int ctx_size);                                   //接收缓存量
    void recv_kv_from_net(int usedTokens);                     // update kv from llama.cpp server timings
    void onSlotAssigned(int slotId);                            // server slot id notification
    void recv_reasoning_tokens(int tokens);                     // capture <think> token count of this turn
    void recv_monitor_decode_ok();

    //处理expend信号的槽
    void recv_speechdecode_over(QString result);
    void recv_whisper_modelpath(QString modelpath);   //传递模型路径
    void recv_embeddingdb_describe(QString describe); //传递知识库的描述

    //处理tool信号的槽
    void recv_controller(int num); //传递控制信息

    //自用的槽
  public slots:
    void showImages(QStringList images_filepath);                             //显示文件名和图像
    void switch_lan_change();                                                 //切换行动纲领的语言
    void recv_gpu_status(float vmem, float vramp, float vcore, float vfree_); //更新gpu内存使用率
    void recv_cpu_status(double cpuload, double memload);                     //传递cpu信息

    //自用的槽
  private slots:
    void monitorTime();                       //监视时间到
    void onSplitterMoved(int pos, int index); //分割器被用户拉动时响应
    void stop_recordAudio();                  //停止录音
    void unlockLoad();
    void send_testhandleTimeout();                //链接模式下测试时延迟发送
    void tool_testhandleTimeout();                //链接模式下测试时延迟发送
    void output_scrollBarValueChanged(int value); //输出区滚动条点击事件响应,如果滚动条不在最下面就停止滚动
    void set_set();                               //设置用户设置内容
    void set_date();                              //设置用户约定内容
    void cancel_date();                           //用户取消约定
    void updateGpuStatus();                       //更新gpu内存使用率
    void updateCpuStatus();                       //更新cpu内存使用率
    void date_ui_cancel_button_clicked();         // 约定选项卡取消按钮响应
    void date_ui_confirm_button_clicked();        // 约定选项卡确认按钮响应
    void settings_ui_cancel_button_clicked();
    void settings_ui_confirm_button_clicked();

    void prompt_template_change();                //提示词模板下拉框响应
    void complete_change();                       //补完模式响应
    void chat_change();                           //对话模式响应
    void web_change();                            //服务模式响应
    // 服务模式已移除：server_onProcessStarted/server_onProcessFinished
    void bench_onProcessFinished();               // llama-bench进程结束响应
    void temp_change();                           //温度滑块响应
    void ngl_change();                            // ngl滑块响应
    void nctx_change();                           // nctx滑块响应
    void repeat_change();                         // repeat滑块响应
    void nthread_change();                        // nthread
    void load_handleTimeout();                    //装载动画时间控制
    void load_begin_handleTimeout();              //滑动到最佳动画位置
    void load_over_handleTimeout();               //模型装载完毕动画,滑动条向下滚,一定要滚到最下面才会停
    void decode_handleTimeout();                  //编码动画时间控制
    void on_send_clicked();                       //用户点击发送按钮响应
    void on_load_clicked();                       //用户点击装载按钮响应
    void on_reset_clicked();                      //用户点击遗忘按钮响应
    void on_date_clicked();                       //用户点击约定按钮响应
    void on_set_clicked();                        //用户点击设置按钮响应
    void onShortcutActivated_F1();                //用户按下F1键响应
    void onShortcutActivated_F2();                //用户按下F2键响应
    void onShortcutActivated_CTRL_ENTER();        //用户按下CTRL+ENTER键响应
    void recv_qimagepath(QString cut_imagepath_); //接收传来的图像
    void monitorAudioLevel();                     // 每隔100毫秒刷新一次监视录音

  private:
    void initTextComponentsMemoryPolicy(); // disable undo, set limits
    void resetOutputDocument();            // replace QTextDocument of output
    void resetStateDocument();             // replace QTextDocument of state
  private:
    Ui::Widget *ui;
};

#endif // WIDGET_H

