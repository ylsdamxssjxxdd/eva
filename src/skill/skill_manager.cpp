#include "skill_manager.h"

#include <QCoreApplication>
#include <QDirIterator>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtConcurrent/QtConcurrentRun>
#include <QtGlobal>

#include "src/utils/zip_extractor.h"

namespace
{
constexpr const char *kSkillFileName = "SKILL.md";

QString readTextFile(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (error) *error = QObject::tr("Failed to open %1: %2").arg(path, file.errorString());
        return {};
    }
    QTextStream in(&file);
    in.setCodec("utf-8");
    QString content = in.readAll();
    file.close();
    return content;
}

bool writeTextFile(const QString &path, const QString &content, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
        if (error) *error = QObject::tr("Failed to open %1 for writing: %2").arg(path, file.errorString());
        return false;
    }
    QTextStream out(&file);
    out.setCodec("utf-8");
    out << content;
    file.close();
    return true;
}

QString normalizeNewlines(QString s)
{
    s.replace("\r\n", "\n");
    s.replace('\r', '\n');
    return s;
}

QString stripQuotes(const QString &value)
{
    if (value.size() >= 2)
    {
        const QChar first = value.front();
        const QChar last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\''))
        {
            return value.mid(1, value.size() - 2).trimmed();
        }
    }
    return value.trimmed();
}

bool loadSkillRecord(const QString &skillDir, SkillManager::SkillRecord &record, QString *error)
{
    QFileInfo info(skillDir);
    if (!info.exists() || !info.isDir())
    {
        if (error) *error = QObject::tr("Skill directory missing: %1").arg(skillDir);
        return false;
    }

    const QString skillFile = QDir(skillDir).filePath(QString::fromUtf8(kSkillFileName));
    if (!QFileInfo::exists(skillFile))
    {
        if (error) *error = QObject::tr("SKILL.md missing in %1").arg(skillDir);
        return false;
    }

    QString loadErr;
    const QString content = readTextFile(skillFile, &loadErr);
    if (content.isEmpty() && !loadErr.isEmpty())
    {
        if (error) *error = loadErr;
        return false;
    }

    QString frontmatterBody;
    const QString frontmatter = SkillManager::normalizeFrontmatter(content, &frontmatterBody);
    if (frontmatter.isEmpty())
    {
        if (error) *error = QObject::tr("Invalid SKILL.md frontmatter in %1").arg(skillFile);
        return false;
    }

    const QString name = stripQuotes(SkillManager::extractYamlScalar(frontmatterBody, QStringLiteral("name")));
    const QString description = stripQuotes(SkillManager::extractYamlScalar(frontmatterBody, QStringLiteral("description")));
    const QString license = stripQuotes(SkillManager::extractYamlScalar(frontmatterBody, QStringLiteral("license")));

    record.id = name.trimmed();
    record.description = description.trimmed();
    record.license = license.trimmed();
    record.frontmatterBody = frontmatterBody;
    record.skillRootPath = QDir::cleanPath(skillDir);
    record.skillFilePath = QDir::cleanPath(skillFile);
    return true;
}

SkillManager::ImportResult performImportJob(const QString &zipPath, const QString &skillsRoot)
{
    SkillManager::ImportResult result;
    if (zipPath.isEmpty())
    {
        result.message = QObject::tr("No archive selected.");
        return result;
    }
    if (skillsRoot.isEmpty())
    {
        result.message = QObject::tr("Skills directory is not configured.");
        return result;
    }

    QFileInfo info(zipPath);
    if (!info.exists() || !info.isFile())
    {
        result.message = QObject::tr("Archive not found: %1").arg(zipPath);
        return result;
    }

    QDir root(skillsRoot);
    if (!root.exists() && !root.mkpath(QStringLiteral(".")))
    {
        result.message = QObject::tr("Failed to create EVA_SKILLS directory.");
        return result;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid())
    {
        result.message = QObject::tr("Failed to create temporary directory for extraction.");
        return result;
    }

    QString extractionError;
    if (!zip::extractArchive(info.absoluteFilePath(), tempDir.path(), &extractionError))
    {
        result.message = extractionError.isEmpty() ? QObject::tr("Archive extraction failed.") : extractionError;
        return result;
    }

    QString skillFilePath;
    {
        QDirIterator finder(tempDir.path(), QStringList() << QString::fromUtf8(kSkillFileName),
                            QDir::Files, QDirIterator::Subdirectories);
        if (!finder.hasNext())
        {
            result.message = QObject::tr("SKILL.md not found in archive.");
            return result;
        }
        skillFilePath = finder.next();
    }

    const QString skillDir = QFileInfo(skillFilePath).absolutePath();
    SkillManager::SkillRecord imported;
    QString loadError;
    if (!loadSkillRecord(skillDir, imported, &loadError))
    {
        result.message = loadError;
        return result;
    }

    if (imported.id.isEmpty())
    {
        result.message = QObject::tr("Skill name is missing in SKILL.md frontmatter.");
        return result;
    }

    const QString targetDir = QDir(root).filePath(imported.id);
    QString removeError;
    if (QFileInfo::exists(targetDir) && !SkillManager::removeDirectory(targetDir, &removeError))
    {
        result.message = removeError;
        return result;
    }

    QString copyError;
    if (!SkillManager::copyDirectory(skillDir, targetDir, &copyError))
    {
        result.message = copyError;
        return result;
    }

    result.ok = true;
    result.skillId = imported.id;
    result.message = QObject::tr("Skill %1 imported.").arg(imported.id);
    return result;
}
} // namespace

SkillManager::SkillManager(QObject *parent)
    : QObject(parent)
{
    importWatcher_.setParent(this);
    connect(&importWatcher_, &QFutureWatcher<ImportResult>::finished, this, &SkillManager::handleImportFinished);
}

void SkillManager::setApplicationDir(const QString &appDir)
{
    applicationDir_ = QDir::cleanPath(appDir);
    skillsRoot_ = QDir(applicationDir_).filePath(QStringLiteral("EVA_SKILLS"));
    bundledSkillsRoot_ = QDir(applicationDir_).filePath(QStringLiteral("bundled_skills"));
}

bool SkillManager::ensureSkillsRoot()
{
    if (skillsRoot_.isEmpty()) return false;
    QDir dir(skillsRoot_);
    if (dir.exists()) return true;
    return dir.mkpath(QStringLiteral("."));
}

bool SkillManager::loadFromDisk()
{
    if (!ensureSkillsRoot()) return false;
    // Preserve enabled flags across disk reloads
    QSet<QString> previouslyEnabled;
    previouslyEnabled.reserve(skills_.size());
    for (const auto &rec : skills_)
    {
        if (rec.enabled) previouslyEnabled.insert(rec.id);
    }
    QVector<SkillRecord> loaded;
    QString error;

    QDirIterator it(skillsRoot_, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::NoIteratorFlags);
    while (it.hasNext())
    {
        const QString entry = it.next();
        SkillRecord rec;
        if (loadSkillFromDirectory(entry, rec, &error))
        {
            loaded.push_back(rec);
        }
        else
        {
            qWarning() << "[SkillManager] skip skill at" << entry << ":" << error;
        }
    }

    skills_ = loaded;
    for (auto &rec : skills_)
    {
        rec.enabled = previouslyEnabled.contains(rec.id);
    }
    sortSkills();
    emit skillsChanged();
    return true;
}

SkillManager::ImportResult SkillManager::importSkillArchive(const QString &zipPath)
{
    ImportResult result;
    if (!ensureSkillsRoot())
    {
        result.message = tr("Failed to create EVA_SKILLS directory.");
        finalizeImport(result);
        return result;
    }

    result = performImportJob(zipPath, skillsRoot_);
    finalizeImport(result);
    return result;
}

void SkillManager::installBundledSkills()
{
    if (!ensureSkillsRoot()) return;
    if (bundledSkillsRoot_.isEmpty()) return;

    QDir bundleDir(bundledSkillsRoot_);
    if (!bundleDir.exists()) return;

    const QStringList bundleEntries = bundleDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    if (bundleEntries.isEmpty()) return;

    const QStringList previouslyEnabledList = enabledSkillIds();
    QSet<QString> previouslyEnabled;
    for (const QString &id : previouslyEnabledList) previouslyEnabled.insert(id);

    bool copied = false;
    QString copyError;
    for (const QString &entry : bundleEntries)
    {
        const QString srcPath = bundleDir.filePath(entry);
        const QString skillFile = QDir(srcPath).filePath(QString::fromUtf8(kSkillFileName));
        if (!QFileInfo::exists(skillFile)) continue;

        const QString dstPath = QDir(skillsRoot_).filePath(entry);
        if (QFileInfo::exists(dstPath)) continue;

        copyError.clear();
        if (!copyDirectory(srcPath, dstPath, &copyError))
        {
            const QString message = copyError.isEmpty()
                                        ? tr("Failed to install bundled skill %1.").arg(entry)
                                        : tr("Failed to install bundled skill %1: %2").arg(entry, copyError);
            emit skillOperationFailed(message);
            continue;
        }
        copied = true;
    }

    if (copied)
    {
        loadFromDisk();
        if (!previouslyEnabled.isEmpty()) restoreEnabledSet(previouslyEnabled);
    }
}

bool SkillManager::removeSkill(const QString &skillId, QString *error)
{
    if (skillId.isEmpty())
    {
        if (error) *error = tr("Skill id is empty.");
        return false;
    }
    const QString targetDir = QDir(skillsRoot_).filePath(skillId);
    if (!QFileInfo::exists(targetDir))
    {
        if (error) *error = tr("Skill directory not found: %1").arg(targetDir);
        return false;
    }
    if (!removeDirectory(targetDir, error)) return false;
    loadFromDisk();
    return true;
}

bool SkillManager::setSkillEnabled(const QString &skillId, bool enabled)
{
    bool changed = false;
    for (auto &rec : skills_)
    {
        if (rec.id == skillId)
        {
            if (rec.enabled != enabled)
            {
                rec.enabled = enabled;
                changed = true;
            }
            break;
        }
    }
    if (changed)
    {
        emit skillsChanged();
    }
    return changed;
}

void SkillManager::restoreEnabledSet(const QSet<QString> &enabled)
{
    bool changed = false;
    for (auto &rec : skills_)
    {
        const bool shouldEnable = enabled.contains(rec.id);
        if (rec.enabled != shouldEnable)
        {
            rec.enabled = shouldEnable;
            changed = true;
        }
    }
    if (changed) emit skillsChanged();
}

QStringList SkillManager::enabledSkillIds() const
{
    QStringList ids;
    for (const auto &rec : skills_)
    {
        if (rec.enabled) ids << rec.id;
    }
    ids.sort(Qt::CaseInsensitive);
    return ids;
}

QString SkillManager::composePromptBlock(const QString &engineerWorkDir, bool engineerActive) const
{
    if (!engineerActive) return {};
    QStringList segments;
    QVector<SkillRecord> enabledSkills;
    enabledSkills.reserve(skills_.size());
    for (const auto &rec : skills_)
    {
        if (rec.enabled) enabledSkills.push_back(rec);
    }
    if (enabledSkills.isEmpty()) return {};

    segments << QStringLiteral("[Skill Usage Protocol]");
    segments << QStringLiteral("Follow the Agent Skills specification. Treat each mounted skill as an extension to be activated deliberately.");
    segments << QStringLiteral("Use the YAML frontmatter below to understand when a skill applies. Before relying on a skill, call the `read_file` tool to load its `SKILL.md` for full instructions.");
    segments << QStringLiteral("Run any scripts packaged with a skill via the `execute_command` tool while staying inside the engineer work directory (%1). Reference the absolute skill paths listed below and write all generated artefacts back into the engineer workspace.")
                     .arg(QDir::toNativeSeparators(engineerWorkDir));

    segments << QString();
    segments << QStringLiteral("[Mounted Skills]");
    for (const auto &rec : enabledSkills)
    {
        segments << QString();
        segments << QStringLiteral("- %1").arg(rec.id);
        segments << QStringLiteral("  SKILL.md: %1").arg(QDir::toNativeSeparators(rec.skillFilePath));
        if (!rec.frontmatterBody.isEmpty())
        {
            segments << QStringLiteral("  Frontmatter:");
            const QStringList metaLines = rec.frontmatterBody.split(QChar('\n'));
            for (const QString &metaLine : metaLines)
            {
                segments << QStringLiteral("    %1").arg(metaLine.trimmed());
            }
        }
    }
    return segments.join(QStringLiteral("\n"));
}

bool SkillManager::loadSkillFromDirectory(const QString &skillDir, SkillRecord &record, QString *error) const
{
    return loadSkillRecord(skillDir, record, error);
}

QString SkillManager::normalizeFrontmatter(const QString &rawContent, QString *frontmatterBody)
{
    QString text = normalizeNewlines(rawContent);
    if (!text.startsWith(QStringLiteral("---\n"))) return {};
    const int endIdx = text.indexOf(QStringLiteral("\n---"), 4);
    if (endIdx == -1) return {};
    const QString body = text.mid(4, endIdx - 4);
    if (frontmatterBody) *frontmatterBody = body;
    return QStringLiteral("---\n%1\n---").arg(body);
}

QString SkillManager::extractYamlScalar(const QString &frontmatter, const QString &key)
{
    if (frontmatter.isEmpty()) return {};
    const QString pattern = QStringLiteral("^(%1)\\s*:\\s*(.*)$").arg(QRegularExpression::escape(key));
    QRegularExpression re(pattern, QRegularExpression::MultilineOption);
    QRegularExpressionMatch match = re.match(frontmatter);
    if (!match.hasMatch()) return {};
    QString value = match.captured(2).trimmed();
    if (value == QStringLiteral("|") || value == QStringLiteral(">"))
    {
        // Collect indented lines
        QStringList lines;
        const int start = match.capturedEnd();
        const QString tail = frontmatter.mid(start);
        const QStringList tailLines = tail.split('\n');
        for (const QString &line : tailLines)
        {
            if (line.startsWith(QStringLiteral("  ")) || line.startsWith(QStringLiteral("\t")))
            {
                lines << line.trimmed();
            }
            else if (line.trimmed().isEmpty())
            {
                lines << QString();
            }
            else
            {
                break;
            }
        }
        value = lines.join('\n').trimmed();
    }
    return value;
}

QString SkillManager::sanitizePathForCommand(const QString &path)
{
    QString result = QDir::toNativeSeparators(path);
    if (result.contains(' '))
    {
        if (!result.startsWith(QLatin1Char('"'))) result.prepend('"');
        if (!result.endsWith(QLatin1Char('"'))) result.append('"');
    }
    return result;
}

bool SkillManager::copyDirectory(const QString &sourceDir, const QString &targetDir, QString *error)
{
    QDir src(sourceDir);
    if (!src.exists())
    {
        if (error) *error = QObject::tr("Source directory not found: %1").arg(sourceDir);
        return false;
    }
    QDir target(targetDir);
    if (!target.exists())
    {
        if (!target.mkpath(QStringLiteral(".")))
        {
            if (error) *error = QObject::tr("Failed to create directory: %1").arg(targetDir);
            return false;
        }
    }

    QFileInfoList entries = src.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const QFileInfo &entry : entries)
    {
        const QString srcPath = entry.absoluteFilePath();
        const QString dstPath = QDir(targetDir).filePath(entry.fileName());
        if (entry.isDir())
        {
            if (!copyDirectory(srcPath, dstPath, error)) return false;
        }
        else
        {
            QFile::remove(dstPath);
            if (!QFile::copy(srcPath, dstPath))
            {
                if (error) *error = QObject::tr("Failed to copy %1 -> %2").arg(srcPath, dstPath);
                return false;
            }
        }
    }
    return true;
}

bool SkillManager::removeDirectory(const QString &path, QString *error)
{
    QDir dir(path);
    if (!dir.exists()) return true;
    if (dir.removeRecursively()) return true;
    if (error) *error = QObject::tr("Failed to remove %1").arg(path);
    return false;
}

QString SkillManager::relativeToWorkdir(const QString &engineerWorkDir, const QString &path) const
{
    if (engineerWorkDir.isEmpty()) return {};
    QDir workDir(engineerWorkDir);
    QString rel = workDir.relativeFilePath(path);
    if (rel.startsWith(QStringLiteral("../"))) return {};
    return rel;
}

void SkillManager::sortSkills()
{
    std::sort(skills_.begin(), skills_.end(), [](const SkillRecord &a, const SkillRecord &b)
              { return QString::localeAwareCompare(a.id, b.id) < 0; });
}
void SkillManager::importSkillArchiveAsync(const QString &zipPath)
{
    if (zipPath.isEmpty())
    {
        ImportResult result;
        result.message = tr("No archive selected.");
        finalizeImport(result);
        return;
    }
    pendingImports_.append(zipPath);
    if (!importWatcher_.isRunning()) processNextQueuedImport();
}

void SkillManager::processNextQueuedImport()
{
    if (importWatcher_.isRunning() || pendingImports_.isEmpty()) return;

    if (!ensureSkillsRoot())
    {
        ImportResult result;
        result.message = tr("Failed to create EVA_SKILLS directory.");
        finalizeImport(result);
        pendingImports_.clear();
        return;
    }

    const QString nextPath = pendingImports_.takeFirst();
    auto future = QtConcurrent::run([skillsRoot = skillsRoot_, nextPath]() -> ImportResult
                                    { return performImportJob(nextPath, skillsRoot); });
    importWatcher_.setFuture(future);
}

void SkillManager::handleImportFinished()
{
    const ImportResult result = importWatcher_.result();
    finalizeImport(result);
    if (!pendingImports_.isEmpty()) processNextQueuedImport();
}

void SkillManager::finalizeImport(const ImportResult &result)
{
    if (!result.ok)
    {
        if (!result.message.isEmpty()) emit skillOperationFailed(result.message);
        return;
    }
    loadFromDisk();
    setSkillEnabled(result.skillId, true);
    emit skillImported(result.skillId);
}
