#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QTemporaryDir>
#include <QUuid>
#include <memory>

#include "../common/TestUtils.h"
#include "xtool.h"

namespace eva::test
{

inline void installTestTranslations(xTool &tool)
{
    QJsonObject words;
    const struct
    {
        const char *key;
        const char *value;
    } entries[] = {
        {"return", "return"},
        {"not load tool", "not load tool"},
        {"Please tell user to embed knowledge into the knowledge base first",
         "Please tell user to embed knowledge into the knowledge base first"},
        {"qureying", "qureying"},
        {"qurey&timeuse", "qurey&timeuse"},
        {"Request error", "Request error"},
        {"The query text segment has been embedded", "The query text segment has been embedded"},
        {"dimension", "dimension"},
        {"word vector", "word vector"},
        {"The three text segments with the highest similarity",
         "The three text segments with the highest similarity"},
        {"Number text segment similarity", "Number text segment similarity"},
        {"content", "content"},
        {"Based on this information, reply to the user's previous questions",
         "Based on this information, reply to the user's previous questions"}};

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

inline std::unique_ptr<xTool> createTestTool(const QString &applicationDir, const QString &workDir)
{
    auto tool = std::make_unique<xTool>(applicationDir);
    tool->workDirRoot = workDir;
    installTestTranslations(*tool);
    return tool;
}

inline QString makeUniqueWorkRoot(QTemporaryDir &root)
{
    const QString unique = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return root.filePath(QStringLiteral("work-%1").arg(unique));
}

} // namespace eva::test
