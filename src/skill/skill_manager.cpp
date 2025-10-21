#include "skill_manager.h"

#include <QCoreApplication>
#include <QDirIterator>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtGlobal>

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
} // namespace

SkillManager::SkillManager(QObject *parent)
    : QObject(parent)
{
}

void SkillManager::setApplicationDir(const QString &appDir)
{
    applicationDir_ = QDir::cleanPath(appDir);
    skillsRoot_ = QDir(applicationDir_).filePath(QStringLiteral("EVA_SKILLS"));
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
    sortSkills();
    emit skillsChanged();
    return true;
}

SkillManager::ImportResult SkillManager::importSkillArchive(const QString &zipPath)
{
    ImportResult result;
    result.skillId = QString();
    if (zipPath.isEmpty())
    {
        result.message = tr("No archive selected.");
        return result;
    }
    QFileInfo info(zipPath);
    if (!info.exists() || !info.isFile())
    {
        result.message = tr("Archive not found: %1").arg(zipPath);
        return result;
    }
    if (!ensureSkillsRoot())
    {
        result.message = tr("Failed to create EVA_SKILLS directory.");
        return result;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid())
    {
        result.message = tr("Failed to create temporary directory for extraction.");
        return result;
    }

    QString extractionError;
    if (!extractArchive(info.absoluteFilePath(), tempDir.path(), &extractionError))
    {
        result.message = extractionError.isEmpty() ? tr("Archive extraction failed.") : extractionError;
        return result;
    }

    // Find SKILL.md
    QString skillFilePath;
    {
        QDirIterator finder(tempDir.path(), QStringList() << QString::fromUtf8(kSkillFileName),
                            QDir::Files, QDirIterator::Subdirectories);
        if (!finder.hasNext())
        {
            result.message = tr("SKILL.md not found in archive.");
            return result;
        }
        skillFilePath = finder.next();
    }

    const QString skillDir = QFileInfo(skillFilePath).absolutePath();
    QString loadError;
    SkillRecord imported;
    if (!loadSkillFromDirectory(skillDir, imported, &loadError))
    {
        result.message = loadError;
        return result;
    }

    if (imported.id.isEmpty())
    {
        result.message = tr("Skill name is missing in SKILL.md frontmatter.");
        return result;
    }

    const QString targetDir = QDir(skillsRoot_).filePath(imported.id);
    QString removeError;
    if (QFileInfo::exists(targetDir) && !removeDirectory(targetDir, &removeError))
    {
        result.message = removeError;
        return result;
    }

    QString copyError;
    if (!copyDirectory(skillDir, targetDir, &copyError))
    {
        result.message = copyError;
        return result;
    }

    // Reload from disk to ensure consistent view
    loadFromDisk();
    setSkillEnabled(imported.id, true);
    result.ok = true;
    result.skillId = imported.id;
    result.message = tr("Skill %1 imported.").arg(imported.id);
    emit skillImported(imported.id);
    return result;
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
    segments << QStringLiteral("Follow the Anthropic Agent Skills specification. Treat each mounted skill as an extension to be activated deliberately.");
    segments << QStringLiteral("Use the YAML frontmatter below to understand when a skill applies. Before relying on a skill, call the `read_file` tool to load its `SKILL.md` for full instructions.");
    segments << QStringLiteral("Run any scripts packaged with a skill via the `execute_command` tool while staying inside the engineer work directory (%1). Reference the absolute skill paths listed below and write all generated artefacts back into the engineer workspace.")
                     .arg(QDir::toNativeSeparators(engineerWorkDir));

    for (const auto &rec : enabledSkills)
    {
        segments << QString();
        segments << QStringLiteral("Skill `%1` metadata:").arg(rec.id);
        segments << rec.frontmatterBlock;
        segments << QStringLiteral("Skill root: %1").arg(QDir::toNativeSeparators(rec.skillRootPath));
        segments << QStringLiteral("Load instructions with `read_file` -> %1").arg(QDir::toNativeSeparators(rec.skillFilePath));
    }
    return segments.join(QStringLiteral("\n"));
}

QString SkillManager::composeEngineerAppendix(const QString &engineerWorkDir, bool engineerActive) const
{
    if (!engineerActive) return {};
    QStringList lines;
    QVector<SkillRecord> enabledSkills;
    for (const auto &rec : skills_)
    {
        if (rec.enabled) enabledSkills.push_back(rec);
    }
    if (enabledSkills.isEmpty()) return {};

    lines << QStringLiteral("[Mounted Skill Directories]");
    for (const auto &rec : enabledSkills)
    {
        const QString rel = relativeToWorkdir(engineerWorkDir, rec.skillRootPath);
        const QString skillRoot = QDir::toNativeSeparators(rec.skillRootPath);
        const QString relDisplay = rel.isEmpty() ? QStringLiteral("(outside workdir)") : rel;
        lines << QStringLiteral("- %1 -> %2 (relative: %3)").arg(rec.id, skillRoot, relDisplay);
        lines << QStringLiteral("  SKILL.md -> %1").arg(QDir::toNativeSeparators(rec.skillFilePath));
    }
    const QString scriptsFs = QDir(enabledSkills.first().skillRootPath).filePath("scripts");
    const QString scriptsDisplay = QDir::toNativeSeparators(scriptsFs);
    const QString examplePath = sanitizePathForCommand(QDir(scriptsFs).filePath("your_script.py"));
    lines << QStringLiteral("Scripts reside under %1. Run them from the workdir, for example: `python %2`.")
                 .arg(scriptsDisplay, examplePath);
    return lines.join(QStringLiteral("\n"));
}

bool SkillManager::loadSkillFromDirectory(const QString &skillDir, SkillRecord &record, QString *error) const
{
    QFileInfo info(skillDir);
    if (!info.exists() || !info.isDir())
    {
        if (error) *error = tr("Skill directory missing: %1").arg(skillDir);
        return false;
    }

    const QString skillFile = QDir(skillDir).filePath(QString::fromUtf8(kSkillFileName));
    if (!QFileInfo::exists(skillFile))
    {
        if (error) *error = tr("SKILL.md missing in %1").arg(skillDir);
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
    const QString frontmatter = normalizeFrontmatter(content, &frontmatterBody);
    if (frontmatter.isEmpty())
    {
        if (error) *error = tr("Invalid SKILL.md frontmatter in %1").arg(skillFile);
        return false;
    }

    const QString name = stripQuotes(extractYamlScalar(frontmatterBody, QStringLiteral("name")));
    const QString description = stripQuotes(extractYamlScalar(frontmatterBody, QStringLiteral("description")));
    const QString license = stripQuotes(extractYamlScalar(frontmatterBody, QStringLiteral("license")));

    record.id = name.trimmed();
    record.description = description.trimmed();
    record.license = license.trimmed();
    record.frontmatterBlock = frontmatter;
    record.skillRootPath = QDir::cleanPath(skillDir);
    record.skillFilePath = QDir::cleanPath(skillFile);
    // enabled flag preserved by caller after restore
    return true;
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

bool SkillManager::extractArchive(const QString &zipPath, const QString &destination, QString *error) const
{
    QDir dest(destination);
    dest.removeRecursively();
    if (!dest.mkpath(QStringLiteral(".")))
    {
        if (error) *error = tr("Failed to prepare extraction directory: %1").arg(destination);
        return false;
    }

#ifdef Q_OS_WIN
    auto runPowerShell = [](const QString &script, QString *stdErrOut) -> bool {
        QProcess ps;
        QStringList args{
            QStringLiteral("-NoProfile"),
            QStringLiteral("-NonInteractive"),
            QStringLiteral("-Command"),
            script};
        ps.start(QStringLiteral("powershell"), args);
        if (!ps.waitForStarted())
        {
            if (stdErrOut) *stdErrOut = QObject::tr("Failed to start PowerShell process.");
            return false;
        }
        ps.waitForFinished(-1);
        if (stdErrOut) *stdErrOut = QString::fromLocal8Bit(ps.readAllStandardError()).trimmed();
        return ps.exitStatus() == QProcess::NormalExit && ps.exitCode() == 0;
    };

    const QString escapedZip = QString(zipPath).replace("'", "''");
    const QString escapedDest = QString(destination).replace("'", "''");

    QString stderrPrimary;
    const QString expandArchiveScript = QStringLiteral(
        "try { Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force } catch { Write-Error $_; exit 1 }")
                                            .arg(escapedZip, escapedDest);
    if (runPowerShell(expandArchiveScript, &stderrPrimary)) return true;

    QString stderrFallback;
    const QString fallbackScript = QStringLiteral(
        "$ErrorActionPreference='Stop';"
        "Add-Type -AssemblyName System.IO.Compression.FileSystem;"
        "$zip=[System.IO.Path]::GetFullPath('%1');"
        "$dest=[System.IO.Path]::GetFullPath('%2');"
        "if (!(Test-Path $dest)) { New-Item -ItemType Directory -Force -Path $dest | Out-Null }"
        "[System.IO.Compression.ZipFile]::ExtractToDirectory($zip,$dest);")
                                       .arg(escapedZip, escapedDest);

    if (runPowerShell(fallbackScript, &stderrFallback)) return true;

    if (error)
    {
        QString combined = stderrPrimary;
        if (!stderrFallback.isEmpty())
        {
            if (!combined.isEmpty()) combined.append('\n');
            combined.append(stderrFallback);
        }
        if (combined.isEmpty()) combined = tr("Archive extraction failed.");
        *error = combined;
    }
    return false;
#else
    QProcess process;
    QStringList args{
        QStringLiteral("-o"),
        zipPath,
        QStringLiteral("-d"),
        destination};
    process.start(QStringLiteral("unzip"), args);
    if (!process.waitForStarted())
    {
        if (error) *error = tr("Failed to start archive extraction process.");
        return false;
    }
    process.waitForFinished(-1);
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
        const QString stderrData = QString::fromLocal8Bit(process.readAllStandardError());
        if (error) *error = stderrData.isEmpty() ? tr("Archive extraction failed.") : stderrData.trimmed();
        return false;
    }
    return true;
#endif
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
