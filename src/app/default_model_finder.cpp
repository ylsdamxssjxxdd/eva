#include "default_model_finder.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QStringList>

// 查找目录中最小的文件（按扩展名过滤，可选谓词）。
static QString findSmallestFile(const QString &root,
                                const QStringList &exts,
                                const std::function<bool(const QFileInfo &)> &pred = nullptr)
{
    if (root.isEmpty() || !QDir(root).exists())
        return QString();
    QString best;
    qint64 bestSz = std::numeric_limits<qint64>::max();
    QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        const QString p = it.next();
        QFileInfo fi(p);
        if (!fi.isFile())
            continue;
        const QString suffix = fi.suffix().toLower();
        if (!exts.contains("*." + suffix))
            continue;
        if (pred && !pred(fi))
            continue;
        const qint64 sz = fi.size();
        if (sz > 0 && sz < bestSz)
        {
            best = fi.absoluteFilePath();
            bestSz = sz;
        }
    }
    return best;
}

DefaultModelPaths DefaultModelFinder::discover(const QString &modelsRoot)
{
    DefaultModelPaths out;
    if (modelsRoot.isEmpty() || !QDir(modelsRoot).exists())
        return out;

    // LLM: EVA_MODELS/llm -> smallest .gguf
    out.llmModel = findSmallestFile(QDir(modelsRoot).filePath("llm"), {"*.gguf"});

    // Embedding: EVA_MODELS/embedding -> smallest .gguf
    out.embeddingModel = findSmallestFile(QDir(modelsRoot).filePath("embedding"), {"*.gguf"});

    // Whisper(STT): EVA_MODELS/speech2text -> prefer filenames containing 'whisper'
    const QString sttRoot = QDir(modelsRoot).filePath("speech2text");
    out.whisperModel = findSmallestFile(sttRoot, {"*.bin", "*.gguf"}, [](const QFileInfo &fi)
                                        { return fi.fileName().toLower().contains("whisper"); });

    // TTS: prefer tts.cpp-compatible models (kokoro / tts keyword) under text2speech; fallback to smallest gguf
    auto findTts = [&](const QString &root)
    {
        QString picked = findSmallestFile(root, {"*.gguf"}, [](const QFileInfo &fi)
                                          {
                                              const QString name = fi.fileName().toLower();
                                              return name.contains("kokoro") || name.contains("tts");
                                          });
        if (picked.isEmpty())
            picked = findSmallestFile(root, {"*.gguf"});
        return picked;
    };
    out.ttsModel = findTts(QDir(modelsRoot).filePath("text2speech"));
    if (out.ttsModel.isEmpty())
        out.ttsModel = findTts(sttRoot);

    // SD: Prefer fixed path EVA_MODELS/text2image/sd1.5-anything-3-q8_0.gguf; fallback: smallest .gguf under text2image
    const QString sdFixed = QDir(modelsRoot).filePath("text2image/sd1.5-anything-3-q8_0.gguf");
    if (QFile::exists(sdFixed))
    {
        out.sdModel = QFileInfo(sdFixed).absoluteFilePath();
        out.sdTemplate = "sd1.5-anything-3";
    }
    if (out.sdModel.isEmpty())
    {
        out.sdModel = findSmallestFile(QDir(modelsRoot).filePath("text2image"), {"*.gguf"});
    }
    if (out.sdTemplate.isEmpty() && !out.sdModel.isEmpty())
    {
        out.sdTemplate = "sd1.5-anything-3";
    }

    return out;
}

void DefaultModelFinder::applyToSettings(QSettings &settings, const DefaultModelPaths &paths)
{
    if (!paths.llmModel.isEmpty())
        settings.setValue("modelpath", paths.llmModel);
    if (!paths.embeddingModel.isEmpty())
        settings.setValue("embedding_modelpath", paths.embeddingModel);
    if (!paths.whisperModel.isEmpty())
        settings.setValue("whisper_modelpath", paths.whisperModel);
    if (!paths.ttsModel.isEmpty())
        settings.setValue("ttscpp_modelpath", paths.ttsModel);
    if (!paths.sdModel.isEmpty())
    {
        settings.setValue("sd_modelpath", paths.sdModel);
        if (!paths.sdTemplate.isEmpty())
            settings.setValue("sd_params_template", paths.sdTemplate);
    }
}
