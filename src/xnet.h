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
    // Mark as invokable so QMetaObject::invokeMethod can call it across threads
    Q_INVOKABLE void run();

  public:
    // Public settings (populated via recv_* from UI)
    APIS apis;
    ENDPOINT_DATA endpoint_data; // 端点参数
    QJsonObject wordsObj;                    // 语言资源（key -> [zh,en,ja]）
    int language_flag = EVA_LANG_ZH;         // 界面语言：0=中文，1=英文，2=日文
    bool is_stop = false;    // 终止标签（UI触发）
    bool thinkFlag = false;  // 是否正在思考
    QString current_content; // 模型最近的输出

    // Helpers
    QString jtr(QString customstr);  // 根据language.json(wordsObj)和language_flag找到对应的文字
    QByteArray createChatBody();     // 构造请求的数据体: 对话模式
    QByteArray createCompleteBody(); // 构造请求的数据体: 补完模式

  public slots:
    void recv_data(ENDPOINT_DATA data); // 传递端点参数
    void recv_apis(APIS apis_);         // 传递api设置参数
    void recv_stop(bool stop);          // 传递停止信号
    void recv_language(int language_flag_);
    void recv_turn(quint64 turnId);     // ä¼ é€’å›žåŽç±»ID

  signals:
    void net2ui_state(const QString &state_string, SIGNAL_STATE state = USUAL_SIGNAL);            // 状态
    void net2ui_output(const QString &result, bool is_while = 1, QColor color = QColor(0, 0, 0)); // 输出
    void net2ui_pushover();                                                                       // 推理完成
    void net2ui_kv_tokens(int usedTokens);
    void net2ui_prompt_baseline(int promptTokens); // baseline prompt tokens (LINK mode usage)                                                        // streaming used token count -> UI
    void net2ui_slot_id(int slotId);               // server-assigned slot id for this conversation
    void net2ui_reasoning_tokens(int count);       // tokens generated inside <think>..</think> this turn (approx)
    // Final per-turn speeds from llama.cpp server timings (tokens/second)
    // prompt_per_second = 上文处理速度; predicted_per_second = 文字生成速度
    void net2ui_speeds(double prompt_per_second, double predicted_per_second);
    void net2ui_turn_counters(int cacheTokens, int promptTokens, int predictedTokens);

  private:
    // 网络中断原因，用于区分用户主动停止、工具中断等场景
    enum class AbortReason
    {
        None,
        UserStop,
        Timeout,
        ApiChange,
        ToolStop,
        Other
    };

    // A single QNetworkAccessManager reused to keep TCP connection warm and reduce overhead
    QNetworkAccessManager *nam_ = nullptr; // created in worker thread lazily
    QPointer<QNetworkReply> reply_;

    // Request lifecycle
    bool running_ = false;
    bool aborted_ = false;
    bool firstByteSeen_ = false; // guard for single TTFB start per request
    int tokens_ = 0;
    QByteArray sseBuffer_;
    QElapsedTimer t_all_;            // total duration
    QElapsedTimer t_first_;          // time to first byte
    QTimer *timeoutTimer_ = nullptr; // hard timeout guard, created lazily in worker thread

    // Timings reported by llama.cpp server (see tools/server web UI)
    // Used to compute prompt-processing and generation speeds accurately.
    int promptTokens_ = -1;        // timings.prompt_n
    double promptMs_ = 0.0;        // timings.prompt_ms
    int predictedTokens_ = -1;     // timings.predicted_n
    double predictedMs_ = 0.0;     // timings.predicted_ms
    bool timingsReceived_ = false; // whether timings were seen in SSE stream
    int reasoningTokensTurn_ = 0;  // approx count for <think> tokens this turn
    // Some providers (e.g. xAI, OpenAI reasoning models) stream reasoning in
    // separate fields (e.g. reasoning / reasoning_content) without <think> markers.
    // Track an emulated think region for those to render nicely in UI.
    bool extThinkActive_ = false; // true when we opened a synthetic <think> block
    // Optional direct speeds when provided by server (tokens/second); -1 if unknown
    double promptPerSec_ = -1.0;
    double predictedPerSec_ = -1.0;
    // 速度上报控制：单次请求仅输出一次，便于在工具链中也能稳定回传
    bool speedsEmitted_ = false;
    AbortReason abortReason_ = AbortReason::None;
    // 工具调用停符处理：命中 </tool_call> 后标记并立刻终止当前流，避免模型继续输出干扰工具判定
    bool sawToolStopword_ = false; // 本轮是否已命中工具停符，防止重复中止
    int cacheTokens_ = -1;
    bool totalsEmitted_ = false;
    quint64 turn_id_ = 0; // å½“å‰å›žåŽIDç”¨äºŽæµç¨‹æ‰«æ

    // Keep track of connections to safely disconnect on abort
    QMetaObject::Connection connReadyRead_;
    QMetaObject::Connection connFinished_;
    QMetaObject::Connection connError_;
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
    QMetaObject::Connection connSslErrors_;
#endif

    void resetState();
    void abortActiveReply(AbortReason reason = AbortReason::Other);
    QNetworkRequest buildRequest(const QUrl &url) const;
    void ensureNetObjects();
    void logRequestPayload(const char *modeTag, const QByteArray &body);
    QString turnTag() const;
    void emitFlowLog(const QString &msg, SIGNAL_STATE state = USUAL_SIGNAL);
    void emitSpeedsIfAvailable(bool allowFallback);

  protected:
    void processSsePayload(bool isChat, const QByteArray &payload);
};

#endif // XNET_H
