#ifndef WIDGET_H
#define WIDGET_H

#include <QApplication>
#include <QWidget>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QHostInfo>
#include <QNetworkInterface>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QSlider>
#include <QLineEdit>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QGroupbox>
#include <QCheckBox>
#include <QTextBrowser>
#include <QScrollBar>
#include <QRadioButton>
#include <QScrollBar>
#include <QTimer>
#include <QProcess>
#include <QResource>
#include <thread>
#include <QMenu>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QProgressBar>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QTcpSocket>
#include <QThread>

#include <windows.h>
#include "utils/customplaintextedit.h"
#include "utils/doubleqprogressbar.h"
#include "expend.h"
#include "xconfig.h"//ui和bot都要导入的共有配置

#include "utils/qmarkdowntextedit/qmarkdowntextedit.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();
    bool eventFilter(QObject *obj, QEvent *event) override;// 事件过滤器函数

public:
    //拯救中文
    QJsonObject wordsObj;
    void getWords(QString json_file_path);

    //ui控制相关
    void state_scroll();//向state末尾添加文本并滚动
    void output_scroll(QColor color = QColor(0,0,0));
    QString ui_output,ui_state;
    bool is_stop_output_scroll = false;//输出区滚动标签
    bool is_stop_state_scroll = false;//状态区滚动标签
    QString history_prompt = "";//记录历史约定
    bool is_datereset = false;//从约定传来的重置信号
    Expend *expend_;//了解更多
    QStringList questions;//用户右击的问题
    QScrollBar *output_scrollBar,*state_scrollBar;//输出区,状态区滑动条

    void ui_state_init();//初始界面状态
    void ui_state_loading();//装载中界面状态
    void ui_state_pushing();//推理中界面状态
    void ui_state_servering();//服务中界面状态
    void ui_state_normal();//待机界面状态

    //连续回答
    QStringList query_list;//待回答列表
    bool is_query =false;//连续回答标签

    //模型控制相关
    QMap<QString, DATES> date_map;//约定模板
    void preLoad();//装载前动作
    bool is_load = false;//模型装载标签
    bool is_run = false;//模型运行标签,方便设置界面的状态
    MODE ui_mode = CHAT_;//模型的模式
    bool ui_need_predecode = false;//需要预解码标签
    QString history_lorapath = "";
    QString history_mmprojpath = "";
    QString ui_template = "qwen";//模板
    QString bot_predecode = "";//模型预解码的内容

    DATES ui_DATES;//ui的约定
    SETTINGS ui_SETTINGS;//ui的设置

    QString ui_model_vocab;//模型词表
    int test_tokens=0;//测试生产的token数量
    QStringList ui_model_logs;
    bool load_percent_tag;
    int max_thread = 1;//最大线程数
    int ui_nthread=1;//使用的线程数

    float load_time = 0;
    QTimer *force_unlockload_pTimer;//到时间强制解锁

    //测试相关
    bool is_test  =false;//测试标签
    QElapsedTimer test_time;
    QList<int> test_question_index;//待测试题目索引
    QStringList test_list_question,test_list_answer;//测试题和答案
    void clearQuestionlist();//清空题库
    void readCsvFile(const QString &fileName);
    void getAllFiles(const QString &floderPath);
    QList<QString> childPathList;//子文件夹路径
    QList<QString> filePathList;//绝对文件路径
    void makeTestQuestion(QString dirPath);//构建题库
    void makeTestIndex();//构建出题索引
    float test_score=0;//答对的个数
    float test_count=0;//回答的次数
    bool help_input = false;//是否添加引导题
    QString makeHelpInput();//构建引导题

    //设置按钮相关
    void set_SetDialog();//设置设置选项
    QDialog *set_dialog;
    QGroupBox *sample_box;
    QSlider *temp_slider;
    QLabel *temp_label;
    QSlider *npredict_slider;
    QLabel *npredict_label;
    QLabel *repeat_label;
    QSlider *repeat_slider;

    QGroupBox *decode_box;
#if defined(BODY_USE_CLBLAST) || defined(BODY_USE_CUBLAST)
    QLabel *ngl_label;
    QSlider *ngl_slider;
#endif
    QLabel *nctx_label;
    QSlider *nctx_slider;
    QLabel *nthread_label;
    QSlider *nthread_slider;
    QLabel *batch_label;
    QSlider *batch_slider;

    QLabel *lora_label;
    QLineEdit *lora_LineEdit;
    QLabel *mmproj_label;
    QLineEdit *mmproj_LineEdit;
    void chooseLorapath();
    void chooseMmprojpath();

    QGroupBox *mode_box;

    //约定按钮相关
    void set_DateDialog();//设置约定选项
    QDialog *date_dialog;
    QGroupBox *prompt_box;
    QLabel *system_label;
    QTextEdit *system_TextEdit;
    QRadioButton *complete_btn,*web_btn,*chat_btn;
    
    QLabel *port_label;QLineEdit *port_lineEdit;
    QString ui_port = "8080";
    QString ipAddress = "";
    QString getFirstNonLoopbackIPv4Address();//获取本机第一个ip地址
    bool current_server=false;//从服务模式回来要重载
    void serverControl();
    QLabel *input_pfx_label;
    QLineEdit *input_pfx_LineEdit;
    QLabel *input_sfx_label;
    QLineEdit *input_sfx_LineEdit;
    QLabel *prompt_label;
    QComboBox *prompt_comboBox;
    void change_api_dialog(bool enable);

    QGroupBox *tool_box;
    QCheckBox *calculator_checkbox,*cmd_checkbox,*search_checkbox,*knowledge_checkbox,*positron_checkbox,*llm_checkbox;
    //bool ui_calculator_ischecked,ui_cmd_ischecked,ui_search_ischecked,ui_knowledge_ischecked,ui_positron_ischecked,ui_llm_ischecked;
    QLabel *extra_label;//附加指令，行动纲领
    QPushButton *switch_lan_button;//切换附加指令的语言
    QString ui_extra_lan="zh";
    QTextEdit *extra_TextEdit;
    QString ui_extra_prompt;
    QString ui_system_prompt;
    QString create_extra_prompt();//构建附加指令
    QMap<QString, TOOLS> tool_map;//工具包
    bool is_load_tool = false;//是否挂载了工具
    QStringList JSONparser(QString text);//输出解析器，提取JSON
    QString tool_result;

    //装载动画相关
    int all_fps =142;//总帧数
    int load_percent;//装载百分比
    void load_log_play();//按日志显示装载进度
    QVector<QString> movie_line;
    QVector<QPointF> movie_dot;
    QVector<QColor> movie_color;
    int load_action = 0;//动作计数
    QTimer *load_pTimer,*load_over_pTimer,*load_begin_pTimer;
    void init_move();//初始化动画参数
    void load_move();//下一帧动画
    void load_play();//连续播放
    void set_dotcolor(QTextCharFormat *format, int action);//设置点颜色
    int playlineNumber = 0;//动画播放的起始行

    //解码码动画相关
    QTimer *decode_pTimer;
    int decode_action = 0;//动作计数
    int currnet_LineNumber = 0;//上一次解码动画所在行
    bool is_decode = false;//解码中标签
    void decode_move();//下一帧
    void decode_play();//播放解码中动画

    //系统信息相关
    QString model_memusage="0",ctx_memusage="0";
    QString model_vramusage="0",ctx_vramusage="0";
    FILETIME preidleTime;
    FILETIME prekernelTime;
    FILETIME preuserTime;
    double CalculateCPULoad();
    float vfree=0;//可用显存大小
    int modelsize_MB = 0;//模型占用空间大小
    bool is_first_getvram=true;//%
    bool is_first_getmem=true;//%
    float first_vramp = 0;//%
    float first_memp = 0;//%

    //请求相关
    void setApiDialog();//初始化设置api选项
    void set_api();//应用api设置
    void startConnection(const QString &ip, int port);//检测ip是否通畅
    
    QDialog *api_dialog;
    QLabel *api_ip_label,*api_port_label,*api_chat_label,*api_complete_label;
    QLineEdit *api_ip_LineEdit,*api_port_LineEdit,*api_chat_LineEdit,*api_complete_LineEdit;
    QCheckBox *api_is_cache;//是否缓存上下文
    bool is_api=false;//是否处于api模式
    APIS apis;//api配置参数
    QStringList ui_user_history,ui_assistant_history;
    QString temp_assistant_history="";//临时数据
    void api_addhelpinput();
    QString current_api;//当前负载端点
    float keeptesttime = 0.1;//回应时间/keeptest*100为延迟量
    QTimer *keeptimer;//测试延迟定时器
    QElapsedTimer keeptime;//测量时间
    
    
//发给模型的信号
signals:
    void ui2bot_language(QJsonObject wordsObj_);//传递使用的语言
    void ui2bot_imagepath(QString image_path);//传递图片路径
    void ui2bot_modelpath(QString model_path,bool is_first_load_=false);//传递模型路径
    void ui2bot_loadmodel();//开始装载模型
    void ui2bot_input(INPUTS inputs,bool is_test_);//传递用户输入
    void ui2bot_push();//开始推理
    void ui2bot_stop();//传递推理停止信号
    void ui2bot_reset(bool is_clear_all);//传递重置信号
    void ui2bot_date(DATES date);//传递约定内容
    void ui2bot_set(SETTINGS settings,bool can_reload);//传递设置内容
    void ui2bot_free();//释放
    void ui2bot_help_input(bool add=true);//添加引导题
    void ui2bot_maxngl(int maxngl_);
//发给net的信号
signals:
    void ui2net_push();//开始推理
    void ui2net_data(ENDPOINT_DATA data);//传递端点参数
    void ui2net_apis(APIS apis);//传递api设置参数
    void ui2net_stop(bool stop);//传递停止信号
//发送给tool的信号
    void ui2tool_push();//开始推理
    void ui2tool_func_arg(QStringList func_arg_list);//传递函数名和参数
//自用信号
signals:
    void server_kill();//终止server信号
    void gpu_reflash();//强制刷新gpu信息
    void ui2expend_log(QString logs);
    void ui2expend_vocab(QString vocab_);
//处理模型信号的槽
public slots:
    void recv_predecode(QString bot_predecode_);//传递模型预解码的内容
    void recv_toolpushover(QString tool_result_);//处理tool推理完毕的槽
    void reflash_output(const QString &result,bool is_while, QColor color);//更新输出区,is_while表示从流式输出的token
    void reflash_state(const QString &state_string, STATE state=USUAL_);//更新状态区
    void recv_loadover(bool ok_, float load_time_);//完成加载模型
    void recv_pushover();//推理完毕的后处理
    void recv_stopover();//停止完毕的后处理
    void recv_arrivemaxctx(bool prepush);//模型达到最大上下文的后处理
    void recv_resetover();//重置完毕的后处理
    void recv_reload();//gpu负载层数改变,重载模型
    void recv_setreset();//bot发信号请求ui触发reset
    void recv_datereset();//bot发信号请求ui触发reset
    void recv_device(QString device_);//接受支持设备信息
    void recv_params(PARAMS p);//bot将模型参数传递给ui
    void recv_vocab(QString model_vocab);//接收模型词表
    void recv_kv(float percent, int ctx_size);//接收缓存量
    void recv_tokens(int tokens);//传递测试解码token数量
    void recv_log(QString log);//传递llama.cpp的log
    void recv_play();
#ifdef BODY_USE_CUBLAST
    void recv_gpu_status(float vmem,float vramp, float vcore, float vfree_);//更新gpu内存使用率
#endif

//自用的槽
private slots:
    void unlockLoad();
    void send_testhandleTimeout();//api模式下测试时延迟发送
    void keepConnection();//持续检测ip是否通畅
    void keep_onConnected();//检测ip是否通畅
    void keep_onError(QAbstractSocket::SocketError socketError);//检测ip是否通畅
    void onConnected();//检测ip是否通畅
    void onError(QAbstractSocket::SocketError socketError);//检测ip是否通畅
    void updateStatus(); //更新cpu内存使用率
    void recv_createExpend();//创建扩展窗口
    void output_scrollBarValueChanged(int value);//输出区滚动条点击事件响应,如果滚动条不在最下面就停止滚动
    void state_scrollBarValueChanged(int value);//状态区滚动条点击事件响应,如果滚动条不在最下面就停止滚动
    void set_set();//设置用户设置内容
    void set_date();//设置用户约定内容
    void calculator_change();//选用计算器工具
    void cmd_change();//选用系统终端工具
    void search_change();//选用搜索引擎工具
    void knowledge_change();//选用知识库工具
    void positron_change();//选用阳电子炮工具
    void llm_change();//选用大模型工具
    void switch_lan_change();//切换行动纲领的语言
    bool toolcheckbox_checked();//判断是否挂载了工具

    void prompt_template_change();//提示词模板下拉框响应
    void complete_change();//补完按钮响应
    void chat_change();//对话按钮响应
    void web_change();//网页服务响应
    void server_onProcessStarted();//进程开始响应
    void server_onProcessFinished();//进程结束响应
    void temp_change();//温度滑块响应
    void ngl_change();//ngl滑块响应
    void batch_change();//batch滑块响应
    void nctx_change();//nctx滑块响应
    void repeat_change();//repeat滑块响应
    void npredict_change();//最大输出长度响应
    void nthread_change();//nthread
    void load_handleTimeout();//装载动画时间控制
    void load_begin_handleTimeout();//滑动到最佳动画位置
    void load_over_handleTimeout();//模型装载完毕动画,滑动条向下滚,一定要滚到最下面才会停
    void decode_handleTimeout();//编码动画时间控制
    void on_send_clicked();//用户点击发送按钮响应
    void on_load_clicked();//用户点击装载按钮响应
    void on_reset_clicked();//用户点击遗忘按钮响应
    void on_date_clicked();//用户点击约定按钮响应
    void on_set_clicked();//用户点击设置按钮响应


private:
    Ui::Widget *ui;
};

#endif // WIDGET_H
