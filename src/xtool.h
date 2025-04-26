#ifndef XTOOL_H
#define XTOOL_H
#include <math.h>

#include <QAbstractSocket>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QProcess>
#include <QScriptEngine>
#include <QTextCodec>
#include <QThread>
#include <QTime>
#include <QTimer>

#include "xconfig.h"

class xTool : public QObject {
    Q_OBJECT
   public:
    xTool(QString applicationDirPath_ = "./");
    ~xTool();
    QString applicationDirPath;
    //拯救中文
    QJsonObject wordsObj;
    int language_flag = 0;
    QString jtr(QString customstr);                    // 根据language.json(wordsObj)和language_flag中找到对应的文字
    void Exec(QPair<QString, QString> func_arg_list);  // 运行

   public:
    QString shell = DEFAULT_SHELL;
    QString pythonExecutable = DEFAULT_PYTHON;
    QPair<QString, QString> func_arg_list;
    bool createTempDirectory(const QString& path);  //创建临时文件夹
    int embedding_server_dim = 1024;  //开启嵌入服务的嵌入维度
    int embedding_server_resultnumb = 3; // 嵌入结果返回个数
    QVector<Embedding_vector> Embedding_DB;              //嵌入的所有文本段的词向量，向量数据库
    QString embedding_query_process(QString query_str);  //获取查询词向量和计算相似度，返回匹配的文本段
    Embedding_vector query_embedding_vector;             //查询词向量
    QString ipAddress = "";
    QString getFirstNonLoopbackIPv4Address();
    double cosine_similarity_1024(const std::vector<double>& a, const std::vector<double>& b);
    std::vector<std::pair<int, double>> similar_indices(const std::vector<double>& user_vector, const QVector<Embedding_vector>& embedding_DB);

   public slots:
    void recv_embedding_resultnumb(int resultnumb);
    void recv_embeddingdb(QVector<Embedding_vector> Embedding_DB_);
    void recv_drawover(QString result_, bool ok_);              //接收图像绘制完成信号
    void tool2ui_controller_over(QString result);               //传递控制完成结果
    void recv_language(int language_flag_);
    void recv_mcpcallover(QString result);
   signals:
    void tool2expend_mcpcall(QString tool_name, QString tool_args);
    void tool2ui_pushover(QString tool_result);
    void tool2ui_state(const QString& state_string, SIGNAL_STATE state = USUAL_SIGNAL);  //发送的状态信号
    void tool2expend_draw(QString prompt_);
    void tool2ui_controller(int num);  //传递控制信息
};

#endif  // XTOOL_H
