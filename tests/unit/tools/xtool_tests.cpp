#include <QApplication>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#include <memory>

#include "../common/TestHarness.h"
#include "xtool.h"

using eva::test::createTestTool;
using eva::test::makeToolCall;
using eva::test::makeUniqueWorkRoot;

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

class XToolPtcTest : public QObject
{
    Q_OBJECT

  private:
    static QString resolvePythonSpec()
    {
        const QStringList candidates = {QStringLiteral("python3"), QStringLiteral("python"), QStringLiteral("py")};
        for (const QString &candidate : candidates)
        {
            const QString exe = QStandardPaths::findExecutable(candidate);
            if (exe.isEmpty()) continue;
            if (candidate == QStringLiteral("py"))
            {
                return QStringLiteral("%1 -3").arg(exe);
            }
            return exe;
        }
        return {};
    }

  private slots:
    void ptcExecutesScript();
    void ptcRejectsInvalidFilename();
};

void XToolPtcTest::ptcExecutesScript()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Failed to create temporary directory for ptc test");

    const QString workRoot = makeUniqueWorkRoot(tempDir);
    QVERIFY2(QDir().mkpath(workRoot), "Failed to create work root for ptc test");

    auto tool = createTestTool(tempDir.path(), workRoot);
    const QString pythonSpec = resolvePythonSpec();
    if (pythonSpec.isEmpty())
    {
        QSKIP("Python interpreter not available; skipping ptc script execution test");
    }
    tool->pythonExecutable = pythonSpec;

    QSignalSpy pushSpy(tool.get(), &xTool::tool2ui_pushover);
    const QString scriptBody = QStringLiteral(
        "import pathlib\n"
        "print('PTC_OK')\n"
        "print(pathlib.Path('.').resolve())\n");

    tool->Exec(makeToolCall("ptc",
                            mcp::json::object({{"filename", "helper_ptc.py"},
                                               {"workdir", "."},
                                               {"content", scriptBody.toStdString()}})));

    QVERIFY2(pushSpy.count() > 0 || pushSpy.wait(5000), "ptc script test produced no push message");
    const QString message = pushSpy.takeFirst().at(0).toString();
    qInfo() << "ptc output:" << message;
    QVERIFY2(message.contains(QStringLiteral("exit code")), "ptc script output missing exit code");

    QFile saved(QDir(workRoot).filePath(QStringLiteral("ptc_temp/helper_ptc.py")));
    QVERIFY2(saved.exists(), "ptc script was not persisted under ptc_temp");
}

void XToolPtcTest::ptcRejectsInvalidFilename()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Failed to create temporary directory for ptc guard test");

    auto tool = createTestTool(tempDir.path(), makeUniqueWorkRoot(tempDir));
    QSignalSpy pushSpy(tool.get(), &xTool::tool2ui_pushover);

    tool->Exec(makeToolCall("ptc",
                            mcp::json::object({{"filename", "../hack.py"},
                                               {"workdir", "."},
                                               {"content", "print('oops')"}})));

    QVERIFY2(pushSpy.count() > 0 || pushSpy.wait(2000), "ptc guard test produced no push message");
    const QString message = pushSpy.takeFirst().at(0).toString();
    QVERIFY2(message.contains(QStringLiteral("filename")), "ptc guard did not report filename validation error");
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

    QFile otherFile(QDir(workRoot).filePath(QStringLiteral("notes/other.txt")));
    QVERIFY2(otherFile.open(QIODevice::WriteOnly | QIODevice::Text), "Failed to create secondary file for read_file batch test");
    otherFile.write("One\nTwo\nThree\n");
    otherFile.close();

    // read_file
    tool->Exec(makeToolCall("read_file", mcp::json::object({
                                                  {"path", "notes/test.txt"},
                                                  {"start_line", 2},
                                                  {"end_line", 3}})));
    const QString readMsg = nextPush("read_file");
    QVERIFY2(readMsg.contains(QStringLiteral(">>> notes/test.txt")), "read_file missing header with path");
    QVERIFY2(readMsg.contains(QStringLiteral("2: Beta")), "read_file missing expected line number/content");
    QVERIFY2(readMsg.contains(QStringLiteral("3: Gamma")), "read_file missing expected content");

    // batched read_file with multiple files and ranges
    tool->Exec(makeToolCall("read_file", mcp::json::object({
                                                  {"files", mcp::json::array({
                                                               mcp::json::object({{"path", "notes/test.txt"}, {"start_line", 1}, {"end_line", 1}}),
                                                               mcp::json::object({{"path", "notes/other.txt"}, {"line_ranges", mcp::json::array({mcp::json::array({2, 3})})}})
                                                           })}})));
    const QString readBatchMsg = nextPush("read_file batch");
    QVERIFY2(readBatchMsg.contains(QStringLiteral(">>> notes/test.txt")), "batched read_file missing first file header");
    QVERIFY2(readBatchMsg.contains(QStringLiteral("1: Alpha")), "batched read_file missing first file content");
    QVERIFY2(readBatchMsg.contains(QStringLiteral(">>> notes/other.txt")), "batched read_file missing second file header");
    QVERIFY2(readBatchMsg.contains(QStringLiteral("2: Two")), "batched read_file missing second file range content");
    QVERIFY2(readMsg.contains(QStringLiteral("Beta")), "read_file missing expected content");
    QVERIFY2(readMsg.contains(QStringLiteral("Gamma")), "read_file missing expected content");

    // replace_in_file
    tool->Exec(makeToolCall("replace_in_file", mcp::json::object({
                                                      {"path", "notes/test.txt"},
                                                      {"old_string", "Beta"},
                                                      {"new_string", "Delta"}})));
    const QString replaceMsg = nextPush("replace_in_file");
    QVERIFY2(replaceMsg.contains(QStringLiteral("replaced 1 occurrence")), "replace_in_file did not report replacement");

    // edit_in_file
    tool->Exec(makeToolCall("edit_in_file", mcp::json::object({
                                                     {"path", "notes/test.txt"},
                                                     {"edits", mcp::json::array({
                                                                   mcp::json::object({
                                                                       {"action", "insert_after"},
                                                                       {"start_line", 2},
                                                                       {"new_content", "BetaPrime"}
                                                                   }),
                                                                   mcp::json::object({
                                                                       {"action", "replace"},
                                                                       {"start_line", 1},
                                                                       {"end_line", 1},
                                                                       {"new_content", "AlphaPrime"}
                                                                   })})},
                                                     {"ensure_newline_at_eof", true}})));
    const QString structuredMsg = nextPush("edit_in_file");
    QVERIFY2(structuredMsg.contains(QStringLiteral("applied 2 edit")), "edit_in_file did not confirm edits");
    QVERIFY2(structuredMsg.contains(QStringLiteral("replace:1")), "edit_in_file summary missing replace count");
    QVERIFY2(structuredMsg.contains(QStringLiteral("insert_after:1")), "edit_in_file summary missing insert count");

    QFile resultFile(QDir(workRoot).filePath(QStringLiteral("notes/test.txt")));
    QVERIFY2(resultFile.open(QIODevice::ReadOnly | QIODevice::Text), "Failed to open edited file for verification");
    const QString finalText = QString::fromUtf8(resultFile.readAll());
    QCOMPARE(finalText, QStringLiteral("AlphaPrime\nDelta\nBetaPrime\nGamma\n"));

    // list_files (default path should point at work root)
    tool->Exec(makeToolCall("list_files", mcp::json::object()));
    const QString listDefaultMsg = nextPush("list_files default");
    QVERIFY2(listDefaultMsg.contains(QStringLiteral("notes/")),
             "list_files default listing should include newly created directory");

    // list_files (explicit directory)
    tool->Exec(makeToolCall("list_files", mcp::json::object({{"path", "notes"}})));
    const QString listMsg = nextPush("list_files");
    QVERIFY2(listMsg.contains(QStringLiteral("notes/test.txt")), "list_files missing expected entry");

    // search_content
    tool->Exec(makeToolCall("search_content", mcp::json::object({{"query", "Delta"}})));
    const QString searchMsg = nextPush("search_content");
    const bool hasSlashPath = searchMsg.contains(QStringLiteral("notes/test.txt")) || searchMsg.contains(QStringLiteral("notes\\test.txt"));
    QVERIFY2(hasSlashPath, "search_content missing file reference");
    QVERIFY2(searchMsg.contains(QStringLiteral("Delta")), "search_content missing match text");
    QVERIFY2(searchMsg.contains(QStringLiteral("Found")), "search_content should include summary");
}

class XToolFileGuardsTest : public QObject
{
    Q_OBJECT

  private slots:
    void replaceInFileEnforcesExpectedCount();
    void replaceInFileShowsSnippetWhenMissing();
};

void XToolFileGuardsTest::replaceInFileEnforcesExpectedCount()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Failed to create temporary directory for replace_in_file guard test");

    const QString workRoot = makeUniqueWorkRoot(tempDir);
    QVERIFY2(QDir().mkpath(workRoot), "Failed to create work root for guard test");
    QDir rootDir(workRoot);
    QVERIFY2(rootDir.mkpath(QStringLiteral("notes")), "Failed to create notes directory for guard test");

    QFile file(rootDir.filePath(QStringLiteral("notes/sample.txt")));
    QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Text), "Failed to prime sample file for guard test");
    file.write("Alpha\nBeta\nGamma\n");
    file.close();

    auto tool = createTestTool(tempDir.path(), workRoot);
    QSignalSpy pushSpy(tool.get(), &xTool::tool2ui_pushover);

    tool->Exec(makeToolCall("replace_in_file",
                            mcp::json::object({{"path", "notes/sample.txt"},
                                               {"old_string", "Beta"},
                                               {"new_string", "BetaPrime"},
                                               {"expected_replacements", 2}})));

    QVERIFY2(pushSpy.count() > 0 || pushSpy.wait(2000), "replace_in_file guard test did not emit push message");
    const QString message = pushSpy.takeFirst().at(0).toString();
    QVERIFY2(message.contains(QStringLiteral("Expected 2 replacement(s) but found 1")),
             "replace_in_file guard did not report expected replacement mismatch");

    QFile verify(rootDir.filePath(QStringLiteral("notes/sample.txt")));
    QVERIFY2(verify.open(QIODevice::ReadOnly | QIODevice::Text), "Failed to reopen file after guard execution");
    const QString persisted = QString::fromUtf8(verify.readAll());
    QCOMPARE(persisted, QStringLiteral("Alpha\nBeta\nGamma\n"));
}

void XToolFileGuardsTest::replaceInFileShowsSnippetWhenMissing()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Failed to create temporary directory for snippet guard test");

    const QString workRoot = makeUniqueWorkRoot(tempDir);
    QVERIFY2(QDir().mkpath(workRoot), "Failed to create work root for snippet guard test");
    QDir rootDir(workRoot);
    QVERIFY2(rootDir.mkpath(QStringLiteral("notes")), "Failed to create notes directory for snippet guard test");

    QFile file(rootDir.filePath(QStringLiteral("notes/sample.txt")));
    QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Text), "Failed to prime sample file for snippet guard test");
    file.write("Alpha\nBeta\nGamma\n");
    file.close();

    auto tool = createTestTool(tempDir.path(), workRoot);
    QSignalSpy pushSpy(tool.get(), &xTool::tool2ui_pushover);

    tool->Exec(makeToolCall("replace_in_file",
                            mcp::json::object({{"path", "notes/sample.txt"},
                                               {"old_string", "BetaPrime block"},
                                               {"new_string", "BetaPrime"}})));

    QVERIFY2(pushSpy.count() > 0 || pushSpy.wait(2000), "replace_in_file missing-match test did not emit push message");
    const QString message = pushSpy.takeFirst().at(0).toString();
    QVERIFY2(message.contains(QStringLiteral("old_string NOT found.")),
             "replace_in_file missing-match flow did not report the failure");
    QVERIFY2(message.contains(QStringLiteral("Snippet: BetaPrime block")),
             "replace_in_file missing-match flow should include snippet preview");
    QVERIFY2(message.contains(QStringLiteral("Hint: provide more surrounding context")),
             "replace_in_file missing-match flow should include hint text");
}

class XToolSearchContentTest : public QObject
{
    Q_OBJECT

  private slots:
    void searchContentHandlesEmptyQueryAndNoMatches();
};

void XToolSearchContentTest::searchContentHandlesEmptyQueryAndNoMatches()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Failed to create temporary directory for search_content guard test");

    const QString workRoot = makeUniqueWorkRoot(tempDir);
    QVERIFY2(QDir().mkpath(workRoot), "Failed to create work root for search_content guard test");
    QDir rootDir(workRoot);
    QVERIFY2(rootDir.mkpath(QStringLiteral("notes")), "Failed to create notes directory for search_content guard test");

    QFile file(rootDir.filePath(QStringLiteral("notes/log.txt")));
    QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Text), "Failed to prime log file for search_content");
    file.write("Alpha bravo charlie");
    file.close();

    auto tool = createTestTool(tempDir.path(), workRoot);
    QSignalSpy pushSpy(tool.get(), &xTool::tool2ui_pushover);

    tool->Exec(makeToolCall("search_content", mcp::json::object({{"query", "   "}})));
    QVERIFY2(pushSpy.count() > 0 || pushSpy.wait(2000), "search_content empty-query test produced no message");
    QString message = pushSpy.takeFirst().at(0).toString();
    QVERIFY2(message.contains(QStringLiteral("Empty query.")),
             "search_content empty-query flow should report validation error");

    pushSpy.clear();
    tool->Exec(makeToolCall("search_content", mcp::json::object({{"query", "delta"}})));
    QVERIFY2(pushSpy.count() > 0 || pushSpy.wait(2000), "search_content no-match test produced no message");
    message = pushSpy.takeFirst().at(0).toString();
    QVERIFY2(message.contains(QStringLiteral("No matches.")),
             "search_content no-match flow should mention the empty result");
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

class XToolWorkdirTest : public QObject
{
    Q_OBJECT

  private slots:
    void recvWorkdirUpdatesRoot();
    void createTempDirectoryHandlesExistingPaths();
};

void XToolWorkdirTest::recvWorkdirUpdatesRoot()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Failed to create temporary directory for workdir test");

    auto tool = createTestTool(tempDir.path(), makeUniqueWorkRoot(tempDir));
    QSignalSpy stateSpy(tool.get(), &xTool::tool2ui_state);

    const QString newRoot = tempDir.filePath(QStringLiteral("custom_root"));
    tool->recv_workdir(newRoot);
    QCOMPARE(tool->workDirRoot, QDir::cleanPath(newRoot));

    const bool stateOk = stateSpy.count() > 0 || stateSpy.wait(1000);
    QVERIFY2(stateOk, "recv_workdir did not emit a state notification");

    const QString message = stateSpy.takeFirst().at(0).toString();
    QVERIFY2(message.contains(QDir::cleanPath(newRoot)), "State notification missing updated path");
}

void XToolWorkdirTest::createTempDirectoryHandlesExistingPaths()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Failed to create temporary directory for temp dir test");

    auto tool = createTestTool(tempDir.path(), makeUniqueWorkRoot(tempDir));
    const QString tempPath = tempDir.filePath(QStringLiteral("EVA_TEMP/work-subdir"));
    QVERIFY2(tool->createTempDirectory(tempPath), "Expected createTempDirectory to create new path");
    QVERIFY2(QDir(tempPath).exists(), "Expected new temporary directory to exist on disk");
    QVERIFY2(!tool->createTempDirectory(tempPath), "Existing directories should not be recreated");
}

class XToolClampTest : public QObject
{
    Q_OBJECT

  private slots:
    void readFileOutputIsClamped();
};

void XToolClampTest::readFileOutputIsClamped()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Failed to create temporary directory for clamp test");

    const QString workRoot = makeUniqueWorkRoot(tempDir);
    QVERIFY2(QDir().mkpath(workRoot), "Failed to create work root for clamp test");
    QDir workDir(workRoot);
    QVERIFY2(workDir.mkpath(QStringLiteral("notes")), "Failed to create notes directory");

    QFile bigFile(workDir.filePath(QStringLiteral("notes/big.txt")));
    QVERIFY2(bigFile.open(QIODevice::WriteOnly | QIODevice::Text), "Failed to open big.txt for writing");
    QByteArray payload("HEAD-");
    payload += QByteArray(60000, 'B');
    payload += "-TAIL";
    QVERIFY2(bigFile.write(payload) == payload.size(), "Failed to write payload for clamp test");
    bigFile.close();

    auto tool = createTestTool(tempDir.path(), workRoot);
    QSignalSpy pushSpy(tool.get(), &xTool::tool2ui_pushover);

    tool->Exec(makeToolCall("read_file", mcp::json::object({{"path", "notes/big.txt"}})));

    if (!(pushSpy.count() > 0 || pushSpy.wait(2000)))
    {
        QFAIL("Expected push message for clamp test");
        return;
    }

    const QString message = pushSpy.takeFirst().at(0).toString();
    QVERIFY2(message.contains(QStringLiteral("[tool output truncated")), "Expected clamp indicator missing");
    QVERIFY2(message.contains(QStringLiteral("...")), "Expected ellipsis marker in clamped output");
    QVERIFY2(message.contains(QStringLiteral("HEAD-")), "Clamped output should keep leading context");
    QVERIFY2(message.contains(QStringLiteral("-TAIL")), "Clamped output should keep trailing context");
}

int main(int argc, char **argv)
{
#ifdef Q_OS_LINUX
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
#endif
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
        XToolPtcTest tc;
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
        XToolFileGuardsTest tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        XToolSearchContentTest tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        XToolMcpListTest tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        XToolWorkdirTest tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        XToolClampTest tc;
        status |= QTest::qExec(&tc, argc, argv);
    }

    return status;
}

#include "xtool_tests.moc"
