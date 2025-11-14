#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QtTest/QtTest>

namespace
{
QString scenarioRoot()
{
#ifdef EVA_FUNCTIONAL_SCENARIO_DIR
    return QString::fromUtf8(EVA_FUNCTIONAL_SCENARIO_DIR);
#else
    return QString();
#endif
}

struct Scenario
{
    QString name;
    QString program;
    QStringList arguments;
    int timeoutMs = 60000;
    QString expectStdout;
    int expectExitCode = 0;

    static Scenario fromJson(const QString &fileName, const QJsonObject &object)
    {
        Scenario scenario;
        scenario.name = object.value(QStringLiteral("name")).toString(fileName);

        const auto commandArray = object.value(QStringLiteral("command")).toArray();
        if (commandArray.isEmpty())
        {
            return scenario;
        }

        scenario.program = commandArray.first().toString();
        for (int i = 1; i < commandArray.size(); ++i)
        {
            scenario.arguments.append(commandArray.at(i).toString());
        }

        scenario.timeoutMs = object.value(QStringLiteral("timeout_ms")).toInt(60000);
        scenario.expectStdout = object.value(QStringLiteral("expect_stdout_contains")).toString();
        scenario.expectExitCode = object.value(QStringLiteral("expect_exit_code")).toInt(0);
        return scenario;
    }
};

} // namespace

class FunctionalScenarioTests : public QObject
{
    Q_OBJECT

  private slots:
    void executeScenarios();
};

void FunctionalScenarioTests::executeScenarios()
{
    const auto root = scenarioRoot();
    QVERIFY2(!root.isEmpty(), "EVA_FUNCTIONAL_SCENARIO_DIR is not defined");

    QDir dir(root);
    QVERIFY2(dir.exists(), qPrintable(QStringLiteral("Scenario directory missing: %1").arg(root)));

    const auto files = dir.entryList(QStringList() << QStringLiteral("*.json"), QDir::Files | QDir::Readable);
    QVERIFY2(!files.isEmpty(), "No functional test scenarios were found");

    for (const auto &file : files)
    {
        QFile handle(dir.filePath(file));
        QVERIFY2(handle.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("Failed to open scenario: %1").arg(file)));

        const auto document = QJsonDocument::fromJson(handle.readAll());
        QVERIFY2(!document.isNull() && document.isObject(),
                 qPrintable(QStringLiteral("Scenario JSON is invalid: %1").arg(file)));

        const auto scenario = Scenario::fromJson(file, document.object());
        QVERIFY2(!scenario.program.isEmpty(),
                 qPrintable(QStringLiteral("Scenario missing command: %1").arg(file)));

        QProcess process;
        process.setProgram(scenario.program);
        process.setArguments(scenario.arguments);
        process.start();

        QVERIFY2(process.waitForStarted(), qPrintable(QStringLiteral("Failed to start scenario: %1").arg(scenario.name)));
        QVERIFY2(process.waitForFinished(scenario.timeoutMs),
                 qPrintable(QStringLiteral("Scenario timed out: %1").arg(scenario.name)));

        QCOMPARE(process.exitCode(), scenario.expectExitCode);

        if (!scenario.expectStdout.isEmpty())
        {
            const QString stdoutText = QString::fromUtf8(process.readAllStandardOutput());
            QVERIFY2(stdoutText.contains(scenario.expectStdout),
                     qPrintable(QStringLiteral("Stdout missing expectation for scenario %1").arg(scenario.name)));
        }
    }
}

QTEST_GUILESS_MAIN(FunctionalScenarioTests)
#include "functional_smoke_tests.moc"
