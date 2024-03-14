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
    QJsonObject wordsObj;
    QString vocab;
    QString model_logs;
    bool is_first_show_expend = true;
    bool is_first_show_vocab = true;
    bool is_first_show_logs = true;
    bool is_first_show_info = true;
    bool load_percent_tag = false;
    void init_expend();//初始化扩展窗口
    bool createTempDirectory(const QString &path);
    QString customOpenfile(QString dirpath, QString describe, QString format);

signals:    
    void expend2ui_state(QString state_string,STATE state);
public slots:
    void recv_log(QString log);
    void recv_vocab(QString vocab);    
    void recv_expend_show(int index_);//通知显示扩展窗口
private slots:
    void on_tabWidget_tabBarClicked(int index);//用户切换选项卡时响应    
private:
    Ui::Expend *ui;
//-------------------------------------------------------------------------
//----------------------------------声音相关--------------------------------
//-------------------------------------------------------------------------
public:
    Whisper_Params whisper_params;//whisper.exe可以传入的参数
    int max_thread;
    QElapsedTimer whisper_time;//计时器
    bool is_first_choose_whispermodel=true;//第一次选好模型路径直接关闭扩展窗口
signals:
    void whisper_kill();
    void expend2ui_voicedecode_over(QString result);
    void expend2ui_whisper_modelpath(QString modelpath);
public slots:
    void recv_voicedecode(QString wavpath);//开始语音转文字
    void whisper_onProcessStarted();
    void whisper_onProcessFinished();
private slots:    
    void on_voice_load_modelpath_button_clicked();//用户点击选择whisper路径时响应
//-------------------------------------------------------------------------
//----------------------------------知识库相关--------------------------------
//-------------------------------------------------------------------------
public:
    Embedding_Params embedding_params;
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
signals:
    void expend2tool_embeddingdb(QVector<Embedding_vector> Embedding_DB_);//发送已嵌入文本段数据给tool
    void expend2ui_embeddingdb_describe(QString describe);//传递知识库的描述
    void expend2tool_serverip(QString serverip);//传递嵌入服务地址
    void expend2tool_serverapi(QString serverapi);//传递嵌入服务端点
public slots:
    void server_onProcessStarted();//进程开始响应
    void server_onProcessFinished();//进程结束响应
private slots:
    void on_embedding_txt_modelpath_button_clicked();//用户点击选择嵌入模型路径时响应
    void on_embedding_txt_upload_clicked();//用户点击上传路径时响应
    void on_embedding_txt_embedding_clicked();//用户点击嵌入时响应
    void on_embedding_test_pushButton_clicked();//用户点击检索时响应
    void on_embedding_txt_modepath_lineedit_textChanged();//嵌入端点改变响应
    void on_embedding_txt_describe_lineEdit_textChanged();//知识库描述改变响应

};

#endif // EXPEND_H
