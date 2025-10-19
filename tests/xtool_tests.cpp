#include <QApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#include <QUuid>
#include <memory>

#include "xtool.h"

namespace
{
mcp::json makeToolCall(const std::string &name, const mcp::json &arguments)
{
    mcp::json call = mcp::json::object();
    call["name"] = name;
    call["arguments"] = arguments;
    return call;
}

void installTestTranslations(xTool &tool)
{
    QJsonObject words;
    const struct
    {
        const char *key;
        const char *value;
    } entries[] = {
        {"return", "return"},
        {"not load tool", "not load tool"},
        {"Please tell user to embed knowledge into the knowledge base first", "Please tell user to embed knowledge into the knowledge base first"},
        {"qureying", "qureying"},
        {"qurey&timeuse", "qurey&timeuse"},
        {"Request error", "Request error"},
        {"The query text segment has been embedded", "The query text segment has been embedded"},
        {"dimension", "dimension"},
        {"word vector", "word vector"},
        {"The three text segments with the highest similarity", "The three text segments with the highest similarity"},
        {"Number text segment similarity", "Number text segment similarity"},
        {"content", "content"},
        {"Based on this information, reply to the user's previous questions", "Based on this information, reply to the user's previous questions"}};

    auto appendTranslation = [&](const QString &key, const QString &value) {
        QJsonArray arr;
        arr.append(value);
        words.insert(key, arr);
    };

    for (const auto &entry : entries)
    {
        appendTranslation(QString::fromUtf8(entry.key), QString::fromUtf8(entry.value));
    }

    tool.wordsObj = words;
    tool.language_flag = 0;
}

std::unique_ptr<xTool> createTestTool(const QString &applicationDir, const QString &workDir)
{
    auto tool = std::make_unique<xTool>(applicationDir);
    tool->workDirRoot = workDir;
    installTestTranslations(*tool);
    return tool;
}

QString makeUniqueWorkRoot(QTemporaryDir &root)
{
    const QString unique = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return root.filePath(QStringLiteral("work-%1").arg(unique));
}

} // namespace

class XToolCalculatorTest : public QObject
{
    Q_OBJECT

  private slots:
    void calculatorProducesResult();
};

void XToolCalculatorTest::calculatorProducesResult()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Failed to create temporary directory for calculator test");

    auto tool = createTestTool(tempDir.path(), makeUniqueWorkRoot(tempDir));

    QSignalSpy pushSpy(tool.get(), &xTool::tool2ui_pushover);

    const mcp::json args = mcp::json::object({{"expression", "1 + 2"}}); // NOLINT
    tool->Exec(makeToolCall("calculator", args));

    const bool pushOk = pushSpy.count() > 0 || pushSpy.wait(2000);
    QVERIFY2(pushOk, "calculator tool did not produce a push notification");

    const auto firstPush = pushSpy.takeFirst();
    const QString pushMessage = firstPush.at(0).toString();
    QVERIFY2(pushMessage.contains(QStringLiteral("calculator")),
             "Push message missing tool name for calculator");
    QVERIFY2(pushMessage.contains(QStringLiteral("3")),
             "Calculator result not present in push message");
}

class XToolExecuteCommandTest : public QObject
{
    Q_OBJECT

  private slots:
    void executeCommandSendsTerminalSignals();
};

void XToolExecuteCommandTest::executeCommandSendsTerminalSignals()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Failed to create temporary directory for execute_command test");

    auto tool = createTestTool(tempDir.path(), makeUniqueWorkRoot(tempDir));

    QSignalSpy startSpy(tool.get(), &xTool::tool2ui_terminalCommandStarted);
    QSignalSpy stdoutSpy(tool.get(), &xTool::tool2ui_terminalStdout);
    QSignalSpy finishedSpy(tool.get(), &xTool::tool2ui_terminalCommandFinished);
    QSignalSpy pushSpy(tool.get(), &xTool::tool2ui_pushover);

    const mcp::json args = mcp::json::object({{"content", "echo EVA_TEST_OUTPUT"}}); // NOLINT
    tool->Exec(makeToolCall("execute_command", args));

    const bool startOk = startSpy.count() > 0 || startSpy.wait(2000);
    QVERIFY2(startOk, "execute_command did not emit the start signal");

    const bool finishedOk = finishedSpy.count() > 0 || finishedSpy.wait(5000);
    QVERIFY2(finishedOk, "execute_command did not finish within timeout");

    if (stdoutSpy.count() == 0)
    {
        stdoutSpy.wait(500);
    }

    const bool pushOk = pushSpy.count() > 0 || pushSpy.wait(2000);
    QVERIFY2(pushOk, "execute_command did not emit final push message");

    const QString pushMessage = pushSpy.takeFirst().at(0).toString();
    QVERIFY2(pushMessage.contains(QStringLiteral("EVA_TEST_OUTPUT")),
             "execute_command push message does not contain command output");

    const auto finishedArgs = finishedSpy.takeFirst();
    QCOMPARE(finishedArgs.at(0).toInt(), 0);
    QCOMPARE(finishedArgs.at(1).toBool(), false);
}

class XToolMcpFlowTest : public QObject
{
    Q_OBJECT

  private slots:
    void mcpToolCallRoundtrip();
};

void XToolMcpFlowTest::mcpToolCallRoundtrip()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Failed to create temporary directory for MCP flow test");

    auto tool = createTestTool(tempDir.path(), makeUniqueWorkRoot(tempDir));

    QSignalSpy pushSpy(tool.get(), &xTool::tool2ui_pushover);

    bool callEmitted = false;
    bool resultHandled = false;
    quint64 invocationId = 0;
    QString emittedName;
    QString emittedArgs;

    QObject::connect(tool.get(), &xTool::tool2mcp_toolcall, tool.get(),
                     [&](quint64 id, const QString &name, const QString &args) {
                         callEmitted = true;
                         invocationId = id;
                         emittedName = name;
                         emittedArgs = args;
                         tool->recv_callTool_over(id, QStringLiteral("mcp-test-result"));
                         resultHandled = true;
                     });

    const mcp::json args = mcp::json::object({{"input", "value"}}); // NOLINT
    tool->Exec(makeToolCall("service@tool_name", args));

    QTRY_VERIFY_WITH_TIMEOUT(callEmitted, 2000);
    QTRY_VERIFY_WITH_TIMEOUT(resultHandled, 2000);
    QVERIFY2(invocationId > 0, "MCP invocation id should be positive");
    QCOMPARE(emittedName, QStringLiteral("service@tool_name"));
    QVERIFY2(emittedArgs.contains(QStringLiteral("\"input\"")),
             "MCP call arguments missing expected payload");

    const bool pushOk = pushSpy.count() > 0 || pushSpy.wait(2000);
    QVERIFY2(pushOk, "MCP tool call result did not trigger a push message");

    const QString pushMessage = pushSpy.takeFirst().at(0).toString();
    QVERIFY2(pushMessage.contains(QStringLiteral("mcp-test-result")),
             "MCP result push message missing tool response content");
}

class XToolKnowledgeTest : public QObject
{
    Q_OBJECT

  private slots:
    void knowledgeWithoutEmbedding();
};

void XToolKnowledgeTest::knowledgeWithoutEmbedding()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Failed to create temporary directory for knowledge test");

    auto tool = createTestTool(tempDir.path(), makeUniqueWorkRoot(tempDir));
    QSignalSpy pushSpy(tool.get(), &xTool::tool2ui_pushover);

    const mcp::json args = mcp::json::object({{"content", "What is EVA?"}});
    tool->Exec(makeToolCall("knowledge", args));

    const bool pushOk = pushSpy.count() > 0 || pushSpy.wait(2000);
    QVERIFY2(pushOk, "knowledge tool did not produce a push notification");

    const QString pushMessage = pushSpy.takeFirst().at(0).toString();
    QVERIFY2(pushMessage.contains(QStringLiteral("embed knowledge into the knowledge base first")),
             "knowledge tool fallback guidance missing");
}

class XToolStableDiffusionTest : public QObject
{
    Q_OBJECT

  private slots:
    void stableDiffusionDeliversResult();
};

void XToolStableDiffusionTest::stableDiffusionDeliversResult()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Failed to create temporary directory for stablediffusion test");

    auto tool = createTestTool(tempDir.path(), makeUniqueWorkRoot(tempDir));
    QSignalSpy pushSpy(tool.get(), &xTool::tool2ui_pushover);

    bool drawRequested = false;
    QObject::connect(tool.get(), &xTool::tool2expend_draw, tool.get(),
                     [&](quint64 id, const QString &prompt) {
                         Q_UNUSED(prompt);
                         drawRequested = true;
                         tool->recv_drawover(id, QStringLiteral("mock-image.png"), true);
                     });

    const mcp::json args = mcp::json::object({{"prompt", "Unit test mecha concept"}}); // NOLINT
    tool->Exec(makeToolCall("stablediffusion", args));

    QTRY_VERIFY_WITH_TIMEOUT(drawRequested, 2000);
    const bool pushOk = pushSpy.count() > 0 || pushSpy.wait(2000);
    QVERIFY2(pushOk, "stablediffusion did not emit a result");

    const QString pushMessage = pushSpy.takeFirst().at(0).toString();
    QVERIFY2(pushMessage.contains(QStringLiteral("<ylsdamxssjxxdd:showdraw>mock-image.png")),
             "stablediffusion push message missing image marker");
}

class XToolFileToolsTest : public QObject
{
    Q_OBJECT

  private slots:
    void readWriteEditListSearch();
};

void XToolFileToolsTest::readWriteEditListSearch()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Failed to create temporary directory for file tools test");

    const QString workRoot = makeUniqueWorkRoot(tempDir);
    QVERIFY2(QDir().mkpath(workRoot), "Failed to create work root");

    auto tool = createTestTool(tempDir.path(), workRoot);
    QSignalSpy pushSpy(tool.get(), &xTool::tool2ui_pushover);

    auto nextPush = [&](const QString &context) -> QString {
        if (!(pushSpy.count() > 0 || pushSpy.wait(2000)))
        {
            QTest::qFail(qPrintable(context + QStringLiteral(" did not emit a push message")), __FILE__, __LINE__);
            return QString();
        }
        return pushSpy.takeFirst().at(0).toString();
    };

    // write_file
    tool->Exec(makeToolCall("write_file", mcp::json::object({
                                                  {"path", "notes/test.txt"},
                                                  {"content", "Alpha\nBeta\nGamma\n"}})));
    const QString writeMsg = nextPush("write_file");
    QVERIFY2(writeMsg.contains(QStringLiteral("write over")), "write_file did not confirm completion");

    // read_file
    tool->Exec(makeToolCall("read_file", mcp::json::object({
                                                 {"path", "notes/test.txt"},
                                                 {"start_line", 2},
                                                 {"end_line", 3}})));
    const QString readMsg = nextPush("read_file");
    QVERIFY2(readMsg.contains(QStringLiteral("Beta")), "read_file missing expected content");
    QVERIFY2(readMsg.contains(QStringLiteral("Gamma")), "read_file missing expected content");

    // edit_file
    tool->Exec(makeToolCall("edit_file", mcp::json::object({
                                                 {"path", "notes/test.txt"},
                                                 {"old_string", "Beta"},
                                                 {"new_string", "Delta"}})));
    const QString editMsg = nextPush("edit_file");
    QVERIFY2(editMsg.contains(QStringLiteral("replaced 1 occurrence")), "edit_file did not report replacement");

    // list_files
    tool->Exec(makeToolCall("list_files", mcp::json::object({{"path", "notes"}})));
    const QString listMsg = nextPush("list_files");
    QVERIFY2(listMsg.contains(QStringLiteral("notes/test.txt")), "list_files missing expected entry");

    // search_content
    tool->Exec(makeToolCall("search_content", mcp::json::object({{"query", "Delta"}})));
    const QString searchMsg = nextPush("search_content");
    QVERIFY2(searchMsg.contains(QStringLiteral("notes/test.txt")), "search_content missing file reference");
    QVERIFY2(searchMsg.contains(QStringLiteral("Delta")), "search_content missing highlighted text");
}

class XToolMcpListTest : public QObject
{
    Q_OBJECT

  private slots:
    void mcpToolListRoundtrip();
};

void XToolMcpListTest::mcpToolListRoundtrip()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Failed to create temporary directory for MCP list test");

    auto tool = createTestTool(tempDir.path(), makeUniqueWorkRoot(tempDir));
    QSignalSpy pushSpy(tool.get(), &xTool::tool2ui_pushover);

    bool listRequested = false;
    QObject::connect(tool.get(), &xTool::tool2mcp_toollist, tool.get(),
                     [&](quint64 id) {
                         listRequested = true;
                         tool->recv_calllist_over(id);
                     });

    tool->Exec(makeToolCall("mcp_tools_list", mcp::json::object()));

    QTRY_VERIFY_WITH_TIMEOUT(listRequested, 2000);

    const bool pushOk = pushSpy.count() > 0 || pushSpy.wait(2000);
    QVERIFY2(pushOk, "mcp_tools_list did not produce a push message");

    const QString pushMessage = pushSpy.takeFirst().at(0).toString();
    QVERIFY2(pushMessage.contains(QStringLiteral("mcp_tool_list")),
             "mcp_tools_list push message missing identifier");
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    int status = 0;
    {
        XToolCalculatorTest tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        XToolExecuteCommandTest tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        XToolMcpFlowTest tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        XToolKnowledgeTest tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        XToolStableDiffusionTest tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        XToolFileToolsTest tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        XToolMcpListTest tc;
        status |= QTest::qExec(&tc, argc, argv);
    }

    return status;
}

#include "xtool_tests.moc"
