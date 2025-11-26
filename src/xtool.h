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
#include <QPointer>
#include <QProcess>
#include <QTextCodec>
#include <QThread>
#include <QTime>
#include <QTimer>

#include "thirdparty/tinyexpr/tinyexpr.h"
#include "utils/docker_sandbox.h"
#include "xconfig.h"

#include <atomic>
#include <cctype>
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else // ─── Linux X11 ───────────────────────────────────────────────────────
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#ifdef Bool
#undef Bool
#endif
#ifdef None // X11 里也定义了 None，会与 Qt::None 等冲突
#undef None
#endif
#ifdef Status // X11 定义的 Status 与 Qt::Status 冲突
#undef Status
#endif
#endif

class xTool : public QObject
{
    Q_OBJECT
  public:
    xTool(QString applicationDirPath_ = "./");
    ~xTool();
    QString applicationDirPath;
    // Root directory for engineer tools to read/write and execute.
    // Defaults to {applicationDirPath}/EVA_WORK but can be overridden from UI.
    QString workDirRoot;
    // 拯救中文
    QJsonObject wordsObj;
    int language_flag = 0;
    QString jtr(QString customstr);  // 根据language.json(wordsObj)和language_flag中找到对应的文字
    void Exec(mcp::json tools_call); // 运行

  public:
    QString shell = DEFAULT_SHELL;
    QString pythonExecutable = DEFAULT_PYTHON;

    bool createTempDirectory(const QString &path);      // 创建临时文件夹
    int embedding_server_dim = 1024;                    // 开启嵌入服务的嵌入维度
    int embedding_server_resultnumb = 3;                // 嵌入结果返回个数
    QVector<Embedding_vector> Embedding_DB;             // 嵌入的所有文本段的词向量，向量数据库
    QString embedding_query_process(QString query_str); // 获取查询词向量和计算相似度，返回匹配的文本段
    Embedding_vector query_embedding_vector;            // 查询词向量
    QString ipAddress = "";
    QString getFirstNonLoopbackIPv4Address();
    double cosine_similarity_1024(const std::vector<double> &a, const std::vector<double> &b);
    std::vector<std::pair<int, double>> similar_indices(const std::vector<double> &user_vector, const QVector<Embedding_vector> &embedding_DB);
    QString mcpToolParser(mcp::json toolsinfo);
    void excute_sequence(std::vector<std::string> build_in_tool_arg); // 执行行动序列
  public slots:
    void cancelExecuteCommand();
    void cancelActiveTool();
    void recv_embedding_resultnumb(int resultnumb);
    void recv_embeddingdb(QVector<Embedding_vector> Embedding_DB_);
    void recv_drawover(quint64 invocationId, QString result_, bool ok_); // 接收图像绘制完成信号
    void tool2ui_controller_over(QString result);                        // 传递控制完成结果
    void recv_language(int language_flag_);
    void recv_callTool_over(quint64 invocationId, QString result);
    void recv_calllist_over(quint64 invocationId);
    // Update working directory root for engineer tools
    void recv_workdir(QString dir);
    void recv_dockerConfig(bool enabled, QString image, QString workdir);
  signals:
    void tool2ui_terminalCommandStarted(const QString &command, const QString &workingDir);
    void tool2ui_terminalStdout(const QString &chunk);
    void tool2ui_terminalStderr(const QString &chunk);
    void tool2ui_terminalCommandFinished(int exitCode, bool interrupted);
    void tool2ui_dockerStatusChanged(const DockerSandboxStatus &status);
    void tool2mcp_toollist(quint64 invocationId);
    void tool2mcp_toolcall(quint64 invocationId, QString tool_name, QString tool_args);
    void tool2ui_pushover(QString tool_result);
    void tool2ui_state(const QString &state_string, SIGNAL_STATE state = USUAL_SIGNAL); // 发送的状态信号
    void tool2expend_draw(quint64 invocationId, QString prompt_);
    void tool2ui_controller(int num); // Notify drawing progress

  private:
    void sendStateMessage(const QString &message, SIGNAL_STATE state = USUAL_SIGNAL);
    void sendPushMessage(const QString &message);
    struct ToolInvocation;
    using ToolInvocationPtr = std::shared_ptr<ToolInvocation>;

    ToolInvocationPtr createInvocation(mcp::json tools_call);
    void startWorkerInvocation(const ToolInvocationPtr &invocation);
    void runToolWorker(const ToolInvocationPtr &invocation);
    void startExecuteCommand(const ToolInvocationPtr &invocation);
    void handleCommandStdout(const ToolInvocationPtr &invocation, QProcess *process, bool isError);
    void handleCommandFinished(const ToolInvocationPtr &invocation, QProcess *process, int exitCode, QProcess::ExitStatus status);
    void finishInvocation(const ToolInvocationPtr &invocation);
    bool shouldAbort(const ToolInvocationPtr &invocation) const;
    void postFinishCleanup(const ToolInvocationPtr &invocation);
    void handleStableDiffusion(const ToolInvocationPtr &invocation);
    void handleMcpToolList(const ToolInvocationPtr &invocation);
    void handleMcpToolCall(const ToolInvocationPtr &invocation);
    QString resolveWorkRoot() const;
    void ensureWorkdirExists(const QString &work) const;
    void onDockerStatusChanged(const DockerSandboxStatus &status);
    bool dockerSandboxEnabled() const;
    bool ensureDockerSandboxReady(QString *errorMessage);
    QString dockerWorkdirOrFallback(const QString &hostWorkdir) const;
    QString containerPathForHost(const QString &absHostPath) const;
    bool dockerReadTextFile(const QString &absHostPath, QString *content, QString *errorMessage);
    bool dockerWriteTextFile(const QString &absHostPath, const QString &content, QString *errorMessage);
    bool runDockerShellCommand(const QString &shellCommand, QString *stdOut, QString *stdErr, QString *errorMessage, const QByteArray &stdinData = QByteArray()) const;
    ToolInvocationPtr activeInvocation() const;
    void setActiveInvocation(const ToolInvocationPtr &invocation);
    void clearActiveInvocation(const ToolInvocationPtr &invocation);

    ToolInvocationPtr activeCommandInvocation_;
    QProcess *activeCommandProcess_ = nullptr;
    bool activeCommandInterrupted_ = false;

    std::atomic<quint64> nextInvocationId_{1};
    mutable std::mutex invocationMutex_;
    ToolInvocationPtr activeInvocation_;
    std::unordered_map<quint64, std::weak_ptr<ToolInvocation>> pendingDrawInvocations_;
    std::unordered_map<quint64, std::weak_ptr<ToolInvocation>> pendingMcpInvocations_;
    std::unordered_map<quint64, std::weak_ptr<ToolInvocation>> pendingMcpListInvocations_;
    static thread_local ToolInvocation *tlsCurrentInvocation_;
    DockerSandbox *dockerSandbox_ = nullptr;
    DockerSandbox::Config dockerConfig_;
};

// 鼠标键盘工具的函数
// ─────────────────────── 通用小工具 ──────────────────────────────────────────
inline void msleep(unsigned long ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

inline std::vector<std::string> split(const std::string &s, char delim = '+')
{
    std::vector<std::string> out;
    std::string buf;
    for (char ch : s)
    {
        if (ch == delim)
        {
            if (!buf.empty()) out.push_back(buf);
            buf.clear();
        }
        else
            buf.push_back(ch);
    }
    if (!buf.empty()) out.push_back(buf);
    return out;
}

inline std::string toUpper(const std::string &s)
{
    std::string r = s;
    for (char &c : r) c = std::toupper(static_cast<unsigned char>(c));
    return r;
}

// ─────────────────────── 平台封装层 ──────────────────────────────────────────
namespace platform
{
#ifdef _WIN32
// ---------------- Windows ----------------------------------------------------
inline LONG toAbs(int v, int full)
{
    return LONG(65535.0 * v / full);
}

inline void moveCursor(int x, int y)
{
    int sx = GetSystemMetrics(SM_CXSCREEN) - 1;
    int sy = GetSystemMetrics(SM_CYSCREEN) - 1;
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dx = toAbs(x, sx);
    in.mi.dy = toAbs(y, sy);
    in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    SendInput(1, &in, sizeof(in));
}
inline void mouseFlag(DWORD f)
{
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = f;
    SendInput(1, &in, sizeof(in));
}
inline void leftDown(int x, int y)
{
    moveCursor(x, y);
    mouseFlag(MOUSEEVENTF_LEFTDOWN);
}
inline void leftUp()
{
    mouseFlag(MOUSEEVENTF_LEFTUP);
}
inline void rightDown(int x, int y)
{
    moveCursor(x, y);
    mouseFlag(MOUSEEVENTF_RIGHTDOWN);
}
inline void rightUp()
{
    mouseFlag(MOUSEEVENTF_RIGHTUP);
}

inline WORD mapSingleKey(const std::string &k)
{
    if (k.size() == 1)
    {
        char c = std::toupper(k[0]);
        if (c >= 'A' && c <= 'Z') return c; // 直接返回字母的ASCII码
        if (c >= '0' && c <= '9') return c; // 数字键
        // 其他特殊字符可以用VkKeyScanA，但要检查返回值
        SHORT vk = VkKeyScanA(k[0]);
        if (vk != -1) return vk & 0xFF;
        return 0;
    }

    static const std::map<std::string, WORD> spec{
        {"ENTER", VK_RETURN}, {"ESC", VK_ESCAPE}, {"TAB", VK_TAB}, {"SPACE", VK_SPACE}, {"BACKSPACE", VK_BACK}, {"DELETE", VK_DELETE}, {"F1", VK_F1}, {"F2", VK_F2}, {"F3", VK_F3}, {"F4", VK_F4}, {"F5", VK_F5}, {"F6", VK_F6}, {"F7", VK_F7}, {"F8", VK_F8}, {"F9", VK_F9}, {"F10", VK_F10}, {"F11", VK_F11}, {"F12", VK_F12}};
    auto it = spec.find(toUpper(k));
    return it == spec.end() ? 0 : it->second;
}

inline void sendKeyCombo(const std::vector<std::string> &tokens)
{
    if (tokens.empty()) return;

    std::vector<WORD> modifiers;
    WORD mainKey = 0;

    // 分离修饰键和主键
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        std::string t = toUpper(tokens[i]);

        if (t == "CTRL")
            modifiers.push_back(VK_CONTROL);
        else if (t == "SHIFT")
            modifiers.push_back(VK_SHIFT);
        else if (t == "ALT")
            modifiers.push_back(VK_MENU);
        else if (t == "WIN")
            modifiers.push_back(VK_LWIN);
        else
        {
            // 非修饰键当作主键（通常是最后一个）
            mainKey = mapSingleKey(t);
            break; // 找到主键就停止
        }
    }

    if (mainKey == 0) return; // 没有有效的主键

    // 按下修饰键
    for (WORD vk : modifiers)
    {
        INPUT in{};
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = vk;
        SendInput(1, &in, sizeof(in));
    }

    // 按下主键
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = mainKey;
    SendInput(1, &in, sizeof(in));

    // 抬起主键
    in.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &in, sizeof(in));

    // 抬起修饰键（倒序）
    for (auto it = modifiers.rbegin(); it != modifiers.rend(); ++it)
    {
        INPUT in{};
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = *it;
        in.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &in, sizeof(in));
    }
}

#else
// ---------------- Linux X11 ---------------------------------------------------
inline Display *dsp()
{
    static Display *d = XOpenDisplay(nullptr);
    return d;
}
inline void flush()
{
    XFlush(dsp());
}

inline void moveCursor(int x, int y)
{
    XTestFakeMotionEvent(dsp(), -1, x, y, CurrentTime);
    flush();
}
inline void leftDown(int x, int y)
{
    moveCursor(x, y);
    XTestFakeButtonEvent(dsp(), 1, True, CurrentTime);
    flush();
}
inline void leftUp()
{
    XTestFakeButtonEvent(dsp(), 1, False, CurrentTime);
    flush();
}
inline void rightDown(int x, int y)
{
    moveCursor(x, y);
    XTestFakeButtonEvent(dsp(), 3, True, CurrentTime);
    flush();
}
inline void rightUp()
{
    XTestFakeButtonEvent(dsp(), 3, False, CurrentTime);
    flush();
}

inline KeyCode ks(const std::string &name)
{
    static std::map<std::string, KeySym> m = {
        {"CTRL", XK_Control_L}, {"SHIFT", XK_Shift_L}, {"ALT", XK_Alt_L}, {"WIN", XK_Super_L}, {"ENTER", XK_Return}, {"ESC", XK_Escape}, {"TAB", XK_Tab}, {"SPACE", XK_space}, {"BACKSPACE", XK_BackSpace}, {"DELETE", XK_Delete}, {"F1", XK_F1}, {"F2", XK_F2}, {"F3", XK_F3}, {"F4", XK_F4}, {"F5", XK_F5}, {"F6", XK_F6}, {"F7", XK_F7}, {"F8", XK_F8}, {"F9", XK_F9}, {"F10", XK_F10}, {"F11", XK_F11}, {"F12", XK_F12}};

    if (name.size() == 1)
    {
        char c = std::tolower(name[0]); // X11通常用小写
        return XKeysymToKeycode(dsp(), c);
    }

    auto it = m.find(name);
    if (it == m.end()) return 0;
    return XKeysymToKeycode(dsp(), it->second);
}

inline void sendKeyCombo(const std::vector<std::string> &tokens)
{
    if (tokens.empty()) return;

    std::vector<KeyCode> modifiers;
    KeyCode mainKey = 0;

    // 分离修饰键和主键
    for (const auto &token : tokens)
    {
        std::string t = toUpper(token);
        if (t == "CTRL" || t == "SHIFT" || t == "ALT" || t == "WIN")
        {
            KeyCode kc = ks(t);
            if (kc) modifiers.push_back(kc);
        }
        else
        {
            mainKey = ks(t);
            break; // 找到主键就停止
        }
    }

    if (mainKey == 0) return;

    // 按下修饰键
    for (KeyCode kc : modifiers)
    {
        XTestFakeKeyEvent(dsp(), kc, True, CurrentTime);
    }

    // 按下主键
    XTestFakeKeyEvent(dsp(), mainKey, True, CurrentTime);
    flush();

    // 抬起主键
    XTestFakeKeyEvent(dsp(), mainKey, False, CurrentTime);

    // 抬起修饰键（倒序）
    for (auto it = modifiers.rbegin(); it != modifiers.rend(); ++it)
    {
        XTestFakeKeyEvent(dsp(), *it, False, CurrentTime);
    }
    flush();
}

#endif
} // namespace platform

#endif // XTOOL_H
