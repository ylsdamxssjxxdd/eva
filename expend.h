#ifndef EXPEND_H
#define EXPEND_H

#include <QWidget>
#include <QJsonObject>
#include <QScrollBar>
#include <QTimer>
#include <QFileDialog>
#include <QProcess>
#include <QFile>
#include <QTextStream>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QObject>
#include <QDebug>
#include <QEventLoop>
#include <QAbstractSocket>
#include <QNetworkInterface>
#include <QPlainTextEdit>
#include <math.h>
#include <QFileDialog>
#include <QSettings>
#include <QTextToSpeech>
#include <QTextCodec>
#include <QMenu>
#include <QShortcut>
#include <QLabel>
#include <QLineEdit>

#include <windows.h>
#include "xconfig.h"

namespace Ui {
class Expend;
}

class Expend : public QWidget
{
    Q_OBJECT
//-------------------------------------------------------------------------
//----------------------------------界面相关--------------------------------
//-------------------------------------------------------------------------
public:
    Expend(QWidget *parent = nullptr);
    ~Expend();
    void closeEvent(QCloseEvent *event) override;//关闭事件
    bool eventFilter(QObject *obj, QEvent *event) override;// 事件过滤器函数
    QJsonObject wordsObj;
    int language_flag = 0;
    QString vocab;
    QString model_logs;
    bool is_first_show_modelproliferation = true;
    bool is_first_show_expend = true;
    bool is_first_show_vocab = true;
    bool is_first_show_logs = true;
    bool is_first_show_info = true;
    bool load_percent_tag = false;
    void init_expend();//初始化增殖窗口
    bool createTempDirectory(const QString &path);
    QString customOpenfile(QString dirpath, QString describe, QString format);
    void readConfig();//读取配置文件并应用
    QString currentpath = DEFAULT_MODELPATH;
    void showReadme();//展示readme内容

    //搜索框相关
    void set_searchBar();//初始化状态区的搜索框
    void resizeEvent(QResizeEvent *event) override;  // 声明resize事件处理函数
    QLineEdit *search_lineedit;
    QList<QTextCursor> searchResults;  // 存储搜索结果位置
    int current_searchResultIndex;            // 当前结果的索引
    QPushButton *prev_searchButton;
    QPushButton *next_searchButton;
    QFrame *searchBar;
    QLabel *search_label;
    void centerCursorInTextEdit(QTextCursor cursor);
    QTextCharFormat highlightFormat; // 用于所有匹配项
    QTextCharFormat currentHighlightFormat; // 用于当前选中项
signals:    
    void expend2ui_state(QString state_string,STATE state);
public slots:
    void recv_language(int language_flag_);//传递语言标志
    void recv_log(QString log);
    void recv_vocab(QString vocab);    
    void recv_expend_show(int index_);//通知显示增殖窗口
private slots:
    void onsearch_ShortcutActivated();//状态区搜索框快捷键响应
    void searchTextChanged(const QString &text);//搜索框文本改变响应
    void onPrevClicked();//上一个
    void onNextClicked();//下一个
    void on_tabWidget_tabBarClicked(int index);//用户切换选项卡时响应    
private:
    Ui::Expend *ui;
//-------------------------------------------------------------------------
//----------------------------------声转文相关--------------------------------
//-------------------------------------------------------------------------
public:
    QProcess *whisper_process;//用来启动whisper.exe
    Whisper_Params whisper_params;//whisper.exe可以传入的参数
    int max_thread;
    QElapsedTimer whisper_time;//计时器
    QString wavpath;
    bool is_handle_whisper = false;//是否为手动转换
signals:
    void whisper_kill();
    void expend2ui_voicedecode_over(QString result);
    void expend2ui_whisper_modelpath(QString modelpath);
public slots:
    void recv_voicedecode(QString wavpath, QString out_format="txt");//开始语音转文字
    void whisper_onProcessStarted();
    void whisper_onProcessFinished();
private slots:    
    void on_whisper_load_modelpath_button_clicked();//用户点击选择whisper路径时响应
    void on_whisper_wavpath_pushButton_clicked();//用户点击选择wav路径时响应
    void on_whisper_execute_pushbutton_clicked();//用户点击执行转换时响应

//-------------------------------------------------------------------------
//----------------------------------知识库相关--------------------------------
//-------------------------------------------------------------------------
public:
    Embedding_Params embedding_params;
    bool embedding_need = false;//下一次打开是否需要自动构建知识库
    bool embedding_need_auto = false;//下一次打开是否需要自启动嵌入服务
    QProcess *server_process;
    QString ipAddress = "";
    QString embedding_server_api = "";
    QString getFirstNonLoopbackIPv4Address();
    void embedding_server_start();//尝试启动server
    QString txtpath;//用户上传的txt文件路径
    int embedding_server_n_embd = 1024;//开启嵌入服务的嵌入维度
    void preprocessTXT();//预处理文件内容
    int show_chunk_index = 0;//待显示的嵌入文本段的序号
    QVector<Embedding_vector> Embedding_DB;//嵌入的所有文本段的词向量，向量数据库
    Embedding_vector user_embedding_vector;
    double cosine_similarity(const std::array<double, 1024>& a, const std::array<double, 1024>& b);
    std::vector<std::pair<int, double>> similar_indices(const std::array<double, 1024>& user_vector, const QVector<Embedding_vector>& embedding_DB);
    void embedding_processing();//知识库构建过程
signals:
    void expend2tool_embeddingdb(QVector<Embedding_vector> Embedding_DB_);//发送已嵌入文本段数据给tool
    void expend2ui_embeddingdb_describe(QString describe);//传递知识库的描述
    void expend2tool_serverip(QString serverip);//传递嵌入服务地址
    void expend2tool_serverapi(QString serverapi);//传递嵌入服务端点
public slots:
    void server_onProcessStarted();//进程开始响应
    void server_onProcessFinished();//进程结束响应
private slots:
    void show_embedding_txt_wait_menu(const QPoint &pos);//右击表格显示菜单
    void embedding_txt_wait_onAdd();//添加表格
    void embedding_txt_wait_onDelete();//删除表格
    void on_embedding_txt_modelpath_button_clicked();//用户点击选择嵌入模型路径时响应
    void on_embedding_txt_upload_clicked();//用户点击上传文档时响应
    void on_embedding_txt_embedding_clicked();//用户点击嵌入时响应
    void on_embedding_test_pushButton_clicked();//用户点击检索时响应
    void on_embedding_txt_api_lineedit_textChanged();//嵌入端点改变响应
    void on_embedding_txt_describe_lineEdit_textChanged();//知识库描述改变响应

//-------------------------------------------------------------------------
//----------------------------------模型量化相关--------------------------------
//-------------------------------------------------------------------------
public:
    QVector<QuantizeType> quantize_types;//量化方法说明数据
    void show_quantize_types();//展示量化方法
    void output_modelpath_change();//根据待量化模型路径和量化方法填入量化后模型路径
    QProcess *quantize_process;
    void quantize(QString in_modelpath, QString out_modelpath, QString important_datapath, QString quantize_type);
private slots:
    void on_model_quantize_row_modelpath_pushButton_clicked();//用户点击选择待量化模型路径时响应
    void on_model_quantize_important_datapath_pushButton_clicked();//用户点击选择重要性矩阵路径时响应
    void on_model_quantize_row_modelpath_lineedit_textChanged();//待量化模型路径改变响应
    void on_model_quantize_type_currentIndexChanged(int index);//量化方法改变响应
    void on_model_quantize_execute_clicked();//用户点击执行量化按钮时响应
    void quantize_onProcessStarted();//开始信号
    void quantize_onProcessFinished();//结束信号

//-------------------------------------------------------------------------
//----------------------------------文生图相关--------------------------------
//-------------------------------------------------------------------------

public:
    SD_Params sd_params;
    QProcess *sd_process;
    bool is_handle_sd = true;
    QString sd_process_output;
    bool uploadimaging = false;//是否正在上传图像
    QString uploadimagepath = "";//上传图像的路径
    bool img2img =false;//是否是图生图操作
public slots:
    void sd_onProcessStarted();//进程开始响应
    void sd_onProcessFinished();//进程结束响应

    void recv_draw(QString prompt_);//接收到tool的开始绘制图像信号
signals:
    void expend2tool_drawover(QString result_, bool ok_);//绘制完成信号
private slots:
    void on_sd_modelpath_pushButton_clicked();//用户点击选择sd模型路径时响应 
    void on_sd_vaepath_pushButton_clicked();//用户点击选择vae模型路径时响应   
    void on_sd_draw_pushButton_clicked();//用户点击文生图时响应  
    void on_sd_modelpath_lineEdit_textChanged();//sd模型路径改变响应
    void on_sd_uploadimage_textEdit_textChanged();//上传图像文本区改变响应
    void on_sd_draw_pushButton_2_clicked();//用户点击图生图时响应  


//-------------------------------------------------------------------------
//----------------------------------文转声相关--------------------------------
//-------------------------------------------------------------------------

public:
    void get_sys_voice();//获取系统可用声源并设置到combobox
    Voice_Params voice_params;
signals:
    void expend2ui_voiceparams(Voice_Params Voice_Params_);

private slots:
    void voice_enable_change();//用户点击启用声音选项响应
    void voice_source_change();//用户切换音源响应
};



#endif // EXPEND_H
