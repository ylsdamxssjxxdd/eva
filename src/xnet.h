#ifndef XNET_H
#define XNET_H

#include <QColor>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QPointer>
#include <QThread>
#include <QTimer>

#include "xconfig.h" //ui和bot都要导入的共有配置

class xNet : public QObject
{
    Q_OBJECT

  public:
    xNet();
    ~xNet() override;

    // Start one request based on current endpoint_data/apis
    void run();

  public:
    // Public settings (populated via recv_* from UI)
    APIS apis;
    ENDPOINT_DATA endpoint_data; // 端点参数
    QJsonObject wordsObj;
    int language_flag = 0;
    bool is_stop = false;    // 终止标签（UI触发）
    bool thinkFlag = false;  // 是否正在思考
    QString current_content; // 模型最近的输出

    // Helpers
    QString jtr(QString customstr);                    // 根据language.json(wordsObj)和language_flag找到对应的文字
    QByteArray createChatBody();                       // 构造请求的数据体: 对话模式
    QByteArray createCompleteBody();                   // 构造请求的数据体: 补完模式
    QString extractContentFromJson(const QString &js); // 解析delta.content
    QStringList extractAllContent(const QString &data);

  public slots:
    void recv_data(ENDPOINT_DATA data); // 传递端点参数
    void recv_apis(APIS apis_);         // 传递api设置参数
    void recv_stop(bool stop);          // 传递停止信号
    void recv_language(int language_flag_);

  signals:
    void net2ui_state(const QString &state_string, SIGNAL_STATE state = USUAL_SIGNAL);            // 状态
    void net2ui_output(const QString &result, bool is_while = 1, QColor color = QColor(0, 0, 0)); // 输出
    void net2ui_pushover();                                                                       // 推理完成

  private:
    // A single QNetworkAccessManager reused to keep TCP connection warm and reduce overhead
    QNetworkAccessManager *nam_ = nullptr; // created in worker thread lazily
    QPointer<QNetworkReply> reply_;

    // Request lifecycle
    bool running_ = false;
    bool aborted_ = false;
    int tokens_ = 0;
    QByteArray sseBuffer_;
    QElapsedTimer t_all_;   // total duration
    QElapsedTimer t_first_; // time to first byte
    QTimer *timeoutTimer_ = nullptr;   // hard timeout guard, created lazily in worker thread

    void resetState();
    void abortActiveReply();
    QNetworkRequest buildRequest(const QUrl &url) const;
    void ensureNetObjects();
};

#endif // XNET_H
