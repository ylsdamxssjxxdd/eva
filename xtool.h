#ifndef XTOOL_H
#define XTOOL_H
#include <QThread>
#include <QDebug>
#include <QScriptEngine>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QObject>
#include <QFile>
#include <QElapsedTimer>
#include <QProcess>
#include <QTime>
#include <QTimer>
#include <QEventLoop>
#include <QAbstractSocket>
#include <QNetworkInterface>

#include "xconfig.h"
class xTool : public QThread
{
    Q_OBJECT
public:
    xTool();
    ~xTool();
    //拯救中文
    QJsonObject wordsObj;
    void run() override;
public:
    QStringList func_arg_list;
    QTimer *positron_p;//阳电子步枪充能定时器
    void positronShoot();//阳电子步枪发射
    int positron_power=0;//阳电子步枪充能值

    //知识库相关
    QVector<Embedding_vector> Embedding_DB;//嵌入的所有文本段的词向量，向量数据库
    QString embedding_query_process(QString query_str);//获取查询词向量和计算相似度，返回匹配的文本段
    Embedding_vector query_embedding_vector;//查询词向量
    QString ipAddress = "";
    QString getFirstNonLoopbackIPv4Address();
    double cosine_similarity(const std::array<double, 2048>& a, const std::array<double, 2048>& b);
    std::vector<std::pair<int, double>> similar_indices(const std::array<double, 2048>& user_vector, const QVector<Embedding_vector>& embedding_DB);
public slots:
    void recv_func_arg(QStringList func_arg_list);
    void positronPower();//阳电子步枪充能
    void recv_embeddingdb(QVector<Embedding_vector> Embedding_DB_);
    
signals:
    void tool2ui_pushover(QString tool_result);
    void tool2ui_state(const QString &state_string, STATE state=USUAL_);//发送的状态信号
    void positron_starter();

};

#endif // XTOOL_H
