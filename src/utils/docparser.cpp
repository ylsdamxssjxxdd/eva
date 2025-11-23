// docparser.cpp - implementation

#include "docparser.h"

#include "thirdparty/miniz/miniz.h"

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QTextCodec>
#include <QTextStream>
#include <QVector>
#include <QXmlStreamReader>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <memory>

extern "C"
{
#include "xls.h"
}

namespace
{
static QString escapeMarkdownCell(QString text)
{
    text.replace(QStringLiteral("|"), QStringLiteral("\\|"));
    text.replace(QStringLiteral("\r"), QString());
    text.replace(QStringLiteral("\n"), QStringLiteral("<br>"));
    return text.trimmed();
}

static QString makeMarkdownTable(const QList<QStringList> &rows)
{
    if (rows.isEmpty()) return {};
    const int columnCount = rows.first().size();
    auto formatRow = [&](const QStringList &row) {
        QStringList cells;
        cells.reserve(columnCount);
        for (int i = 0; i < columnCount; ++i)
        {
            if (i < row.size())
                cells << escapeMarkdownCell(row.at(i));
            else
                cells << QString();
        }
        return QStringLiteral("| ") + cells.join(QStringLiteral(" | ")) + QStringLiteral(" |");
    };
    QStringList lines;
    lines << formatRow(rows.first());
    QStringList divider;
    for (int i = 0; i < columnCount; ++i) divider << QStringLiteral("---");
    lines << QStringLiteral("| ") + divider.join(QStringLiteral(" | ")) + QStringLiteral(" |");
    for (int r = 1; r < rows.size(); ++r) lines << formatRow(rows.at(r));
    return lines.join(QStringLiteral("\n"));
}

class ZipArchiveReader
{
public:
    ZipArchiveReader() { memset(&m_archive, 0, sizeof(m_archive)); }
    ~ZipArchiveReader() { close(); }

    bool open(const QString &path)
    {
        close();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return false;
        m_data = file.readAll();
        if (m_data.isEmpty()) return false;
        if (!mz_zip_reader_init_mem(&m_archive, m_data.constData(), m_data.size(), 0))
        {
            close();
            return false;
        }
        m_open = true;
        return true;
    }

    void close()
    {
        if (m_open)
        {
            mz_zip_reader_end(&m_archive);
            m_open = false;
        }
        m_data.clear();
    }

    QByteArray fileData(const QString &name) const
    {
        if (!m_open) return {};
        const QByteArray encoded = name.toUtf8();
        const int index = mz_zip_reader_locate_file(const_cast<mz_zip_archive *>(&m_archive), encoded.constData(), nullptr, 0);
        if (index < 0) return {};
        size_t outSize = 0;
        void *ptr = mz_zip_reader_extract_to_heap(const_cast<mz_zip_archive *>(&m_archive), index, &outSize, 0);
        if (!ptr || outSize == 0)
        {
            if (ptr) mz_free(ptr);
            return {};
        }
        QByteArray data(static_cast<const char *>(ptr), static_cast<int>(outSize));
        mz_free(ptr);
        return data;
    }

    QStringList filesWithPrefix(const QString &prefix) const
    {
        QStringList names;
        if (!m_open) return names;
        const int count = static_cast<int>(mz_zip_reader_get_num_files(&m_archive));
        for (int i = 0; i < count; ++i)
        {
            mz_zip_archive_file_stat st;
            if (!mz_zip_reader_file_stat(const_cast<mz_zip_archive *>(&m_archive), i, &st)) continue;
            const QString name = QString::fromUtf8(st.m_filename);
            if (name.startsWith(prefix)) names << name;
        }
        std::sort(names.begin(), names.end());
        return names;
    }

private:
    QByteArray m_data;
    mutable mz_zip_archive m_archive;
    bool m_open = false;
};

constexpr quint32 OleFreeSector = 0xFFFFFFFFu;
constexpr quint32 OleEndOfChain = 0xFFFFFFFEu;
constexpr quint32 OleFatSector = 0xFFFFFFFDu;
constexpr quint32 OleDifSector = 0xFFFFFFFCu;

struct OleDirectoryEntry
{
    QString name;
    quint8 type = 0;
    quint32 startSector = OleEndOfChain;
    quint64 size = 0;
};

class CompoundFileReader
{
public:
    bool load(const QString &path);
    QByteArray streamByName(const QString &name) const;

private:
    int sectorSize() const { return 1 << m_sectorShift; }
    int miniSectorSize() const { return 1 << m_miniSectorShift; }
    QByteArray sectorData(quint32 sector) const;
    int sectorOffset(quint32 sector) const;
    bool appendDifatSector(quint32 sector, QVector<quint32> &difat);
    bool buildFat(const QVector<quint32> &difat);
    bool buildMiniFat(quint32 startSector, quint32 sectorCount);
    bool buildDirectory();
    bool buildMiniStream();
    QByteArray readStream(quint32 startSector, quint64 size, bool useMini) const;
    quint32 nextSector(quint32 current) const;

    QByteArray m_data;
    QVector<quint32> m_fat;
    QVector<quint32> m_miniFat;
    QVector<OleDirectoryEntry> m_entries;
    QByteArray m_miniStream;
    quint16 m_sectorShift = 0;
    quint16 m_miniSectorShift = 0;
    quint16 m_majorVersion = 3;
    quint32 m_miniStreamCutoff = 4096;
    quint32 m_firstDirSector = OleEndOfChain;
    quint32 m_firstMiniFatSector = OleEndOfChain;
    quint32 m_numMiniFatSectors = 0;
    bool m_valid = false;
};

struct TextPiece
{
    quint32 cpStart = 0;
    quint32 cpEnd = 0;
    quint32 fileOffset = 0;
    bool unicode = true;
};

struct FibInfo
{
    quint32 fcMin = 0;
    quint32 fcMac = 0;
    quint32 fcClx = 0;
    quint32 lcbClx = 0;
    bool useTable1 = false;
    bool complex = false;
};

bool CompoundFileReader::load(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    m_data = file.readAll();
    if (m_data.size() < 512) return false;

    const uchar *header = reinterpret_cast<const uchar *>(m_data.constData());
    const quint64 signature = qFromLittleEndian<quint64>(header);
    if (signature != 0xE11AB1A1E011CFD0ULL) return false;

    m_majorVersion = qFromLittleEndian<quint16>(header + 0x1A);
    const quint16 byteOrder = qFromLittleEndian<quint16>(header + 0x1C);
    if (byteOrder != 0xFFFE) return false;

    m_sectorShift = qFromLittleEndian<quint16>(header + 0x1E);
    m_miniSectorShift = qFromLittleEndian<quint16>(header + 0x20);
    if (m_sectorShift < 9 || m_sectorShift > 16) return false;
    if (m_miniSectorShift > m_sectorShift) return false;

    m_miniStreamCutoff = qFromLittleEndian<quint32>(header + 0x38);
    m_firstDirSector = qFromLittleEndian<quint32>(header + 0x30);
    m_firstMiniFatSector = qFromLittleEndian<quint32>(header + 0x3C);
    m_numMiniFatSectors = qFromLittleEndian<quint32>(header + 0x40);
    quint32 firstDifatSector = qFromLittleEndian<quint32>(header + 0x44);
    quint32 numDifatSectors = qFromLittleEndian<quint32>(header + 0x48);

    QVector<quint32> difat;
    difat.reserve(128);
    const uchar *difatHead = header + 0x4C;
    for (int i = 0; i < 109; ++i)
    {
        const quint32 entry = qFromLittleEndian<quint32>(difatHead + i * 4);
        if (entry != OleFreeSector) difat.append(entry);
    }
    quint32 difatSector = firstDifatSector;
    while (numDifatSectors > 0 && difatSector != OleEndOfChain)
    {
        if (!appendDifatSector(difatSector, difat)) return false;
        const QByteArray block = sectorData(difatSector);
        if (block.size() != sectorSize()) return false;
        difatSector = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(block.constData()) + sectorSize() - 4);
        --numDifatSectors;
    }

    if (!buildFat(difat)) return false;
    if (!buildMiniFat(m_firstMiniFatSector, m_numMiniFatSectors)) return false;
    if (!buildDirectory()) return false;
    if (!buildMiniStream()) return false;
    m_valid = true;
    return true;
}

QByteArray CompoundFileReader::streamByName(const QString &name) const
{
    if (!m_valid) return {};
    for (const auto &entry : m_entries)
    {
        if (entry.type != 2) continue;
        if (entry.name.compare(name, Qt::CaseInsensitive) != 0) continue;
        const bool useMini = entry.size < m_miniStreamCutoff && !m_miniStream.isEmpty();
        return readStream(entry.startSector, entry.size, useMini);
    }
    return {};
}

QByteArray CompoundFileReader::sectorData(quint32 sector) const
{
    const int off = sectorOffset(sector);
    if (off < 0) return {};
    return QByteArray(m_data.constData() + off, sectorSize());
}

int CompoundFileReader::sectorOffset(quint32 sector) const
{
    if (sector == OleEndOfChain) return -1;
    const qint64 offset = 512 + static_cast<qint64>(sector) * sectorSize();
    if (offset < 0 || offset + sectorSize() > m_data.size()) return -1;
    return static_cast<int>(offset);
}

bool CompoundFileReader::appendDifatSector(quint32 sector, QVector<quint32> &difat)
{
    QByteArray data = sectorData(sector);
    if (data.size() != sectorSize()) return false;
    const int intsPerSector = sectorSize() / 4;
    const uchar *ptr = reinterpret_cast<const uchar *>(data.constData());
    for (int i = 0; i < intsPerSector - 1; ++i)
    {
        quint32 value = qFromLittleEndian<quint32>(ptr + i * 4);
        if (value != OleFreeSector) difat.append(value);
    }
    return true;
}

bool CompoundFileReader::buildFat(const QVector<quint32> &difat)
{
    if (difat.isEmpty()) return false;
    m_fat.clear();
    const int intsPerSector = sectorSize() / 4;
    for (quint32 sector : difat)
    {
        QByteArray data = sectorData(sector);
        if (data.size() != sectorSize()) return false;
        const uchar *ptr = reinterpret_cast<const uchar *>(data.constData());
        for (int i = 0; i < intsPerSector; ++i)
        {
            m_fat.append(qFromLittleEndian<quint32>(ptr + i * 4));
        }
    }
    return !m_fat.isEmpty();
}

bool CompoundFileReader::buildMiniFat(quint32 startSector, quint32 sectorCount)
{
    m_miniFat.clear();
    if (startSector == OleEndOfChain || sectorCount == 0) return true;
    quint32 sector = startSector;
    const int intsPerSector = sectorSize() / 4;
    for (quint32 i = 0; i < sectorCount && sector != OleEndOfChain; ++i)
    {
        QByteArray data = sectorData(sector);
        if (data.size() != sectorSize()) return false;
        const uchar *ptr = reinterpret_cast<const uchar *>(data.constData());
        for (int j = 0; j < intsPerSector; ++j)
        {
            m_miniFat.append(qFromLittleEndian<quint32>(ptr + j * 4));
        }
        sector = nextSector(sector);
    }
    return true;
}

bool CompoundFileReader::buildDirectory()
{
    QByteArray dirStream = readStream(m_firstDirSector, 0, false);
    if (dirStream.isEmpty()) return false;
    m_entries.clear();
    const int entrySize = 128;
    const int count = dirStream.size() / entrySize;
    m_entries.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        const char *base = dirStream.constData() + i * entrySize;
        const quint16 nameLen = qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(base + 64));
        if (nameLen < 2) continue;
        const int charCount = qBound(0, static_cast<int>(nameLen / 2) - 1, 32);
        QString name = QString::fromUtf16(reinterpret_cast<const ushort *>(base), charCount);
        OleDirectoryEntry entry;
        entry.name = name;
        entry.type = static_cast<quint8>(base[66]);
        entry.startSector = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(base + 116));
        if (m_majorVersion >= 4)
            entry.size = qFromLittleEndian<quint64>(reinterpret_cast<const uchar *>(base + 120));
        else
            entry.size = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(base + 120));
        m_entries.append(entry);
    }
    return !m_entries.isEmpty();
}

bool CompoundFileReader::buildMiniStream()
{
    const auto rootIt = std::find_if(m_entries.begin(), m_entries.end(), [](const OleDirectoryEntry &entry) {
        return entry.type == 5;
    });
    if (rootIt == m_entries.end()) return false;
    m_miniStream = readStream(rootIt->startSector, rootIt->size, false);
    return true;
}

quint32 CompoundFileReader::nextSector(quint32 current) const
{
    if (current >= static_cast<quint32>(m_fat.size())) return OleEndOfChain;
    return m_fat.at(static_cast<int>(current));
}

QByteArray CompoundFileReader::readStream(quint32 startSector, quint64 size, bool useMini) const
{
    QByteArray buffer;
    if (size == 0 && !useMini)
    {
        quint32 sector = startSector;
        const int sz = sectorSize();
        while (sector != OleEndOfChain)
        {
            const int offset = sectorOffset(sector);
            if (offset < 0) break;
            buffer.append(m_data.constData() + offset, sz);
            sector = nextSector(sector);
        }
        return buffer;
    }

    if (useMini)
    {
        quint32 sector = startSector;
        quint64 remaining = size;
        const int miniSz = miniSectorSize();
        while (sector != OleEndOfChain && remaining > 0)
        {
            const qint64 offset = static_cast<qint64>(sector) * miniSz;
            if (offset < 0 || offset + miniSz > m_miniStream.size()) break;
            const int chunk = static_cast<int>(qMin<quint64>(remaining, miniSz));
            buffer.append(m_miniStream.constData() + offset, chunk);
            remaining -= chunk;
            if (sector >= static_cast<quint32>(m_miniFat.size())) break;
            sector = m_miniFat.at(static_cast<int>(sector));
        }
        buffer.truncate(static_cast<int>(size));
        return buffer;
    }

    quint32 sector = startSector;
    quint64 remaining = size;
    const int sectorSz = sectorSize();
    while (sector != OleEndOfChain && remaining > 0)
    {
        const int offset = sectorOffset(sector);
        if (offset < 0) break;
        const int chunk = static_cast<int>(qMin<quint64>(remaining, sectorSz));
        buffer.append(m_data.constData() + offset, chunk);
        remaining -= chunk;
        sector = nextSector(sector);
    }
    buffer.truncate(static_cast<int>(size));
    return buffer;
}

static bool parseFib(const QByteArray &wordStream, FibInfo &info)
{
    if (wordStream.size() < 256) return false;
    const uchar *ptr = reinterpret_cast<const uchar *>(wordStream.constData());
    const quint16 ident = qFromLittleEndian<quint16>(ptr);
    if (ident != 0xA5EC) return false;
    const quint16 flags = qFromLittleEndian<quint16>(ptr + 0x0A);
    info.useTable1 = (flags & 0x0200) != 0;
    info.complex = (flags & 0x0004) != 0;
    info.fcMin = qFromLittleEndian<quint32>(ptr + 0x18);
    info.fcMac = qFromLittleEndian<quint32>(ptr + 0x1C);

    int pos = 32;
    if (wordStream.size() < pos + 2) return false;
    const quint16 csw = qFromLittleEndian<quint16>(ptr + pos);
    pos += 2 + csw * 2;
    if (wordStream.size() < pos + 2) return false;
    const quint16 cslw = qFromLittleEndian<quint16>(ptr + pos);
    pos += 2 + cslw * 4;
    if (wordStream.size() < pos + 2) return false;
    const quint16 cbRgFcLcb = qFromLittleEndian<quint16>(ptr + pos);
    pos += 2;
    if (wordStream.size() < pos + cbRgFcLcb * 8) return false;

    const int idx = 33;
    if (cbRgFcLcb > idx)
    {
        const int offset = pos + idx * 8;
        info.fcClx = qFromLittleEndian<quint32>(ptr + offset);
        info.lcbClx = qFromLittleEndian<quint32>(ptr + offset + 4);
    }
    return true;
}

static QVector<TextPiece> parseTextPieces(const QByteArray &tableStream, quint32 fcClx, quint32 lcbClx)
{
    QVector<TextPiece> pieces;
    if (fcClx == 0 || lcbClx == 0) return pieces;
    if (fcClx + lcbClx > static_cast<quint32>(tableStream.size())) return pieces;
    const uchar *clx = reinterpret_cast<const uchar *>(tableStream.constData() + fcClx);
    int pos = 0;
    while (pos < static_cast<int>(lcbClx))
    {
        const quint8 clxt = clx[pos++];
        if (clxt == 0x01)
        {
            if (pos + 4 > static_cast<int>(lcbClx)) break;
            const quint32 lcb = qFromLittleEndian<quint32>(clx + pos);
            pos += 4;
            if (lcb == 0 || pos + static_cast<int>(lcb) > static_cast<int>(lcbClx)) break;
            const uchar *plc = clx + pos;
            pos += static_cast<int>(lcb);
            const int pieceCount = (static_cast<int>(lcb) - 4) / (8 + 4);
            if (pieceCount <= 0) break;
            QVector<quint32> cps(pieceCount + 1);
            for (int i = 0; i < cps.size(); ++i)
            {
                cps[i] = qFromLittleEndian<quint32>(plc + i * 4);
            }
            const uchar *pcd = plc + (pieceCount + 1) * 4;
            for (int i = 0; i < pieceCount; ++i)
            {
                const quint32 fc = qFromLittleEndian<quint32>(pcd + i * 8 + 2);
                const bool unicode = (fc & 0x40000000u) == 0;
                quint32 fileOffset = unicode ? fc : (fc & 0x3FFFFFFFu) / 2;
                TextPiece piece;
                piece.cpStart = cps[i];
                piece.cpEnd = cps[i + 1];
                piece.fileOffset = fileOffset;
                piece.unicode = unicode;
                pieces.append(piece);
            }
            break;
        }
        else if (clxt == 0x02)
        {
            if (pos + 2 > static_cast<int>(lcbClx)) break;
            const quint16 cb = qFromLittleEndian<quint16>(clx + pos);
            pos += 2 + cb;
        }
        else
        {
            break;
        }
    }
    return pieces;
}

static QString decodePieces(const QByteArray &wordStream, const QVector<TextPiece> &pieces)
{
    QString out;
    if (pieces.isEmpty()) return out;
    QTextCodec *codec = QTextCodec::codecForName("Windows-1252");
    for (const TextPiece &piece : pieces)
    {
        if (piece.cpEnd <= piece.cpStart) continue;
        const quint32 charCount = piece.cpEnd - piece.cpStart;
        const quint32 byteCount = piece.unicode ? charCount * 2 : charCount;
        if (piece.fileOffset + byteCount > static_cast<quint32>(wordStream.size())) continue;
        if (piece.unicode)
        {
            const ushort *src = reinterpret_cast<const ushort *>(wordStream.constData() + piece.fileOffset);
            out.append(QString::fromUtf16(src, static_cast<int>(charCount)));
        }
        else
        {
            const QByteArray bytes(wordStream.constData() + piece.fileOffset, static_cast<int>(byteCount));
            out.append(codec ? codec->toUnicode(bytes) : QString::fromLatin1(bytes));
        }
    }
    return out;
}

static QString decodeSimpleRange(const QByteArray &wordStream, quint32 fcMin, quint32 fcMac)
{
    if (fcMac <= fcMin || fcMin >= static_cast<quint32>(wordStream.size())) return {};
    quint32 limit = qMin(fcMac, static_cast<quint32>(wordStream.size()));
    quint32 span = limit - fcMin;
    if (span < 4) return {};
    if (span % 2 != 0) --span;
    const ushort *src = reinterpret_cast<const ushort *>(wordStream.constData() + fcMin);
    return QString::fromUtf16(src, static_cast<int>(span / 2));
}

static QString normalizeWordText(const QString &raw)
{
    if (raw.isEmpty()) return {};
    QString cleaned;
    cleaned.reserve(raw.size());
    for (QChar ch : raw)
    {
        const ushort code = ch.unicode();
        if (code == 0x0000) continue;
        if (code == 0x0001 || code == 0x0002)
        {
            cleaned += QLatin1Char('\n');
            continue;
        }
        if (code == 0x0007 || code == 0x000B || code == 0x000C || code == 0x001E || code == 0x001F)
        {
            cleaned += QLatin1Char('\n');
            continue;
        }
        if (code == 0x000D)
        {
            cleaned += QLatin1Char('\n');
            continue;
        }
        cleaned += ch;
    }

    QStringList lines;
    QString current;
    auto flushLine = [&]() {
        const QString trimmed = current.trimmed();
        if (!trimmed.isEmpty()) lines << trimmed;
        current.clear();
    };
    for (QChar ch : std::as_const(cleaned))
    {
        if (ch == QLatin1Char('\n'))
            flushLine();
        else if (!ch.isNull())
            current += ch;
    }
    flushLine();
    return lines.join(QStringLiteral("\n"));
}

static QString readWpsViaWordBinary(const QString &path)
{
    CompoundFileReader reader;
    if (!reader.load(path)) return {};
    const QByteArray wordStream = reader.streamByName(QStringLiteral("WordDocument"));
    if (wordStream.isEmpty()) return {};
    FibInfo fib;
    if (!parseFib(wordStream, fib)) return {};
    QByteArray tableStream = reader.streamByName(fib.useTable1 ? QStringLiteral("1Table") : QStringLiteral("0Table"));
    QVector<TextPiece> pieces;
    if (!tableStream.isEmpty() && fib.fcClx && fib.lcbClx)
        pieces = parseTextPieces(tableStream, fib.fcClx, fib.lcbClx);
    QString raw;
    if (!pieces.isEmpty())
        raw = decodePieces(wordStream, pieces);
    if (raw.isEmpty())
        raw = decodeSimpleRange(wordStream, fib.fcMin, fib.fcMac);
    return normalizeWordText(raw);
}

} // namespace

namespace DocParser
{
QString readPlainTextFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QTextStream in(&f);
    in.setCodec("UTF-8");
    const QString s = in.readAll();
    return s;
}

// naive markdown cleanup
QString markdownToText(const QString &md)
{
    QString s = md;
    // remove fenced code blocks
    s.remove(QRegularExpression(QStringLiteral("```[\\s\\S]*?```")));
    // inline code
    s.replace(QRegularExpression("`([^`]*)`"), "\\1");
    // images ![alt](...)
    s.replace(QRegularExpression("!\\[([^\\]]*)\\]\\([^\\)]*\\)"), "\\1");
    // links [text](url) -> text
    s.replace(QRegularExpression("\\[([^\\]]*)\\]\\([^\\)]*\\)"), "\\1");
    // headings leading #'s
    s.replace(QRegularExpression(QStringLiteral("^\\n?\\n?\\s*#+\\s*"), QRegularExpression::MultilineOption), "");
    // blockquotes
    s.replace(QRegularExpression("^>\\s*", QRegularExpression::MultilineOption), "");
    // tables: strip pipes/separators
    s.replace(QRegularExpression("^\\|.*\\|$", QRegularExpression::MultilineOption), "");
    s.replace(QRegularExpression(QStringLiteral("^\\s*\\|?\\s*:-*:?\\s*(\\|\\s*:-*:?\\s*)*$"), QRegularExpression::MultilineOption), "");
    // HTML tags
    s.replace(QRegularExpression("<[^>]+>"), "");
    // emphasis **bold**, *italic*
    s.replace("**", "");
    s.replace("*", "");
    return s;
}

// Extract human-readable text from DOCX document.xml
static QString readDocxParagraphProperties(QXmlStreamReader &xr)
{
    QString style;
    while (!xr.atEnd())
    {
        auto token = xr.readNext();
        if (token == QXmlStreamReader::StartElement && xr.name() == QLatin1String("pStyle"))
        {
            QString value = xr.attributes().value(QStringLiteral("w:val")).toString();
            if (value.isEmpty()) value = xr.attributes().value(QStringLiteral("val")).toString();
            style = value;
        }
        else if (token == QXmlStreamReader::EndElement && xr.name() == QLatin1String("pPr"))
        {
            break;
        }
    }
    return style;
}

static QString readDocxParagraph(QXmlStreamReader &xr, QString *styleOut)
{
    QString text;
    QString style;
    while (!xr.atEnd())
    {
        auto token = xr.readNext();
        if (token == QXmlStreamReader::StartElement)
        {
            const QStringRef name = xr.name();
            if (name == QLatin1String("pPr"))
            {
                style = readDocxParagraphProperties(xr);
            }
            else if (name == QLatin1String("t"))
            {
                text += xr.readElementText(QXmlStreamReader::IncludeChildElements);
            }
            else if (name == QLatin1String("br") || name == QLatin1String("cr"))
            {
                text += QLatin1Char('\n');
            }
            else if (name == QLatin1String("tab"))
            {
                text += QLatin1Char(' ');
            }
        }
        else if (token == QXmlStreamReader::EndElement && xr.name() == QLatin1String("p"))
        {
            break;
        }
    }
    if (styleOut) *styleOut = style;
    return text.trimmed();
}

static QString readDocxTable(QXmlStreamReader &xr);

static QString readDocxTableCell(QXmlStreamReader &xr)
{
    QStringList fragments;
    while (!xr.atEnd())
    {
        auto token = xr.readNext();
        if (token == QXmlStreamReader::StartElement)
        {
            if (xr.name() == QLatin1String("p"))
            {
                QString style;
                const QString paragraph = readDocxParagraph(xr, &style);
                if (!paragraph.isEmpty()) fragments << paragraph;
            }
            else if (xr.name() == QLatin1String("tbl"))
            {
                const QString nested = readDocxTable(xr);
                if (!nested.isEmpty()) fragments << nested;
            }
        }
        else if (token == QXmlStreamReader::EndElement && xr.name() == QLatin1String("tc"))
        {
            break;
        }
    }
    return fragments.join(QStringLiteral("\n")).trimmed();
}

static QStringList readDocxTableRow(QXmlStreamReader &xr)
{
    QStringList cells;
    while (!xr.atEnd())
    {
        auto token = xr.readNext();
        if (token == QXmlStreamReader::StartElement && xr.name() == QLatin1String("tc"))
        {
            cells << readDocxTableCell(xr);
        }
        else if (token == QXmlStreamReader::EndElement && xr.name() == QLatin1String("tr"))
        {
            break;
        }
    }
    return cells;
}

static QString readDocxTable(QXmlStreamReader &xr)
{
    QList<QStringList> rows;
    while (!xr.atEnd())
    {
        auto token = xr.readNext();
        if (token == QXmlStreamReader::StartElement && xr.name() == QLatin1String("tr"))
        {
            const QStringList row = readDocxTableRow(xr);
            if (!row.isEmpty()) rows.append(row);
        }
        else if (token == QXmlStreamReader::EndElement && xr.name() == QLatin1String("tbl"))
        {
            break;
        }
    }
    if (rows.isEmpty()) return {};
    // ensure header width equals max column count
    int maxColumns = 0;
    for (const QStringList &row : std::as_const(rows))
        maxColumns = std::max(maxColumns, row.size());
    if (maxColumns == 0) return {};
    if (rows.first().size() < maxColumns)
    {
        QStringList padded = rows.first();
        padded.reserve(maxColumns);
        while (padded.size() < maxColumns) padded << QString();
        rows.first() = padded;
    }
    return makeMarkdownTable(rows);
}

static QString formatDocxParagraphMarkdown(const QString &text, const QString &style)
{
    if (text.isEmpty()) return {};
    if (!style.isEmpty() && style.startsWith(QLatin1String("Heading"), Qt::CaseInsensitive))
    {
        bool ok = false;
        const int level = qBound(1, style.midRef(7).toInt(&ok), 6);
        if (ok)
        {
            return QString(level, QLatin1Char('#')) + QLatin1Char(' ') + text;
        }
    }
    return text;
}

static QString parseDocxDocumentXml(const QString &xml)
{
    QStringList blocks;
    QXmlStreamReader xr(xml);
    while (!xr.atEnd())
    {
        auto token = xr.readNext();
        if (token == QXmlStreamReader::StartElement)
        {
            if (xr.name() == QLatin1String("p"))
            {
                QString style;
                const QString text = readDocxParagraph(xr, &style);
                const QString block = formatDocxParagraphMarkdown(text, style);
                if (!block.isEmpty()) blocks << block;
            }
            else if (xr.name() == QLatin1String("tbl"))
            {
                const QString table = readDocxTable(xr);
                if (!table.isEmpty()) blocks << table;
            }
        }
    }
    return blocks.join(QStringLiteral("\n\n"));
}

static QString readDocxViaZipArchive(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists() || !info.isFile()) return {};

    QFile archiveFile(info.absoluteFilePath());
    if (!archiveFile.open(QIODevice::ReadOnly)) return {};
    const QByteArray archiveBytes = archiveFile.readAll();
    if (archiveBytes.isEmpty()) return {};

    mz_zip_archive archive;
    memset(&archive, 0, sizeof(archive));
    if (!mz_zip_reader_init_mem(&archive, archiveBytes.constData(), archiveBytes.size(), 0))
    {
        mz_zip_reader_end(&archive);
        return {};
    }

    const int fileIndex = mz_zip_reader_locate_file(&archive, "word/document.xml", nullptr, 0);
    if (fileIndex < 0)
    {
        mz_zip_reader_end(&archive);
        return {};
    }

    size_t xmlSize = 0;
    void *xmlPtr = mz_zip_reader_extract_to_heap(&archive, fileIndex, &xmlSize, 0);
    mz_zip_reader_end(&archive);
    if (!xmlPtr || xmlSize == 0)
    {
        if (xmlPtr) mz_free(xmlPtr);
        return {};
    }
    if (xmlSize > static_cast<size_t>(std::numeric_limits<int>::max()))
    {
        mz_free(xmlPtr);
        return {};
    }

    const QString xml = QString::fromUtf8(static_cast<const char *>(xmlPtr), static_cast<int>(xmlSize));
    mz_free(xmlPtr);
    return parseDocxDocumentXml(xml);
}

QString readDocxText(const QString &path)
{
#ifdef _WIN32
    // Try: read document.xml directly via .NET ZipFile to avoid disk extraction.
    auto psReadDocXml = [&](const QString &psExe) -> QString
    {
        QString pEsc = path;
        pEsc.replace("'", "''");
        const QString script =
            "$ErrorActionPreference='Stop';"
            "[Console]::OutputEncoding=[System.Text.Encoding]::UTF8;"
            "Add-Type -AssemblyName 'System.IO.Compression.FileSystem';"
            "$p='" +
            pEsc + "';"
                   "$zip=[System.IO.Compression.ZipFile]::OpenRead($p);"
                   "try{"
                   "$e=$zip.GetEntry('word/document.xml');"
                   "if($e -eq $null){ exit 2 }"
                   "$sr=New-Object System.IO.StreamReader($e.Open(),[System.Text.Encoding]::UTF8);"
                   "$xml=$sr.ReadToEnd();$sr.Close();"
                   "[Console]::Write($xml)"
                   "}finally{$zip.Dispose()}";
        QProcess pr;
        QStringList args;
        args << QStringLiteral("-NoLogo") << QStringLiteral("-NoProfile") << QStringLiteral("-Command") << script;
        pr.start(psExe, args);
        if (!pr.waitForFinished(20000)) return {};
        if (pr.exitCode() != 0) return {};
        const QByteArray out = pr.readAllStandardOutput();
        return QString::fromUtf8(out);
    };

    QString xml = psReadDocXml(QStringLiteral("powershell"));
    if (xml.isEmpty()) xml = psReadDocXml(QStringLiteral("pwsh"));
    if (!xml.isEmpty()) return parseDocxDocumentXml(xml);
#endif

    return readDocxViaZipArchive(path);
}

static bool isLikelyPrintableWordChar(quint16 ch)
{
    if (ch == 0x0000 || ch == 0xFFFF || ch == 0xFFFE) return false;
    if (ch == 0x0009 || ch == 0x000A || ch == 0x000D) return true;
    if (ch == 0x3000) return true; // ideographic space
    if (ch >= 0x20 && ch <= 0xD7FF) return true;
    if (ch >= 0xE000 && ch <= 0xF8FF) return false; // private use blocks
    if (ch >= 0xF000) return false;
    return false;
}

static bool looksLikeDocumentText(const QString &chunk)
{
    const QString trimmed = chunk.trimmed();
    if (trimmed.size() < 2 || trimmed.size() > 1024) return false;
    static const QSet<QString> noise = {
        QStringLiteral("Root Entry"),
        QStringLiteral("SummaryInformation"),
        QStringLiteral("DocumentSummaryInformation"),
        QStringLiteral("WordDocument"),
        QStringLiteral("0Table"),
        QStringLiteral("1Table"),
        QStringLiteral("Normal.dotm"),
        QStringLiteral("WpsCustomData"),
        QStringLiteral("KSOProductBuildVer"),
        QStringLiteral("KSOTemplateDocerSaveRecord"),
        QStringLiteral("默认段落字体"),
        QStringLiteral("Calibri"),
        QStringLiteral("Calibr"),
        QStringLiteral("普通表格")
    };
    if (noise.contains(trimmed)) return false;
    if (trimmed.contains(QLatin1Char('@'))) return false;

    int nonSpaceLen = 0;
    for (QChar c : trimmed)
    {
        if (!c.isSpace()) ++nonSpaceLen;
    }
    if (nonSpaceLen == 0) return false;

    int cjk = 0;
    int digits = 0;
    int asciiUpper = 0;
    int asciiAlpha = 0;
    int asciiLower = 0;
    for (QChar ch : trimmed)
    {
        if (ch.unicode() >= 0x4E00 && ch.unicode() <= 0x9FFF) ++cjk;
        if (ch.isDigit()) ++digits;
        if (ch.isUpper() && ch.unicode() <= 0x7F) ++asciiUpper;
        if (ch.isLetter() && ch.unicode() <= 0x7F) ++asciiAlpha;
        if (ch.isLower() && ch.unicode() <= 0x7F) ++asciiLower;
    }
    const int important = cjk + digits;
    if (important == 0)
    {
        if (asciiAlpha < 4) return false;
        if (asciiUpper > asciiAlpha / 2 + 2) return false;
        const bool hasWordBreak = trimmed.contains(QLatin1Char(' ')) || trimmed.contains(QLatin1Char('\t')) ||
                                  trimmed.contains(QLatin1Char('-')) || trimmed.contains(QLatin1Char('_'));
        if (asciiLower == 0 && !hasWordBreak && asciiAlpha < 6) return false;
        return true;
    }
    if (important * 2 < nonSpaceLen && cjk == 0) return false;
    if (asciiUpper > important * 2 + 4) return false;
    return true;
}

static int chunkScore(const QString &chunk)
{
    int cjk = 0;
    int digits = 0;
    int asciiAlpha = 0;
    int asciiUpper = 0;
    for (QChar ch : chunk)
    {
        if (ch.unicode() >= 0x4E00 && ch.unicode() <= 0x9FFF) ++cjk;
        if (ch.isDigit()) ++digits;
        if (ch.isLetter() && ch.unicode() <= 0x7F) ++asciiAlpha;
        if (ch.isUpper() && ch.unicode() <= 0x7F) ++asciiUpper;
    }
    int score = cjk * 5 + digits * 3;
    if (asciiAlpha >= 4)
    {
        const int bonus = std::max(1, (asciiAlpha - asciiUpper) / 3);
        score += bonus;
    }
    if (digits >= 6 && digits >= cjk && digits > asciiAlpha) score += digits * 10;
    if (score == 0 && asciiAlpha >= 4) score = asciiAlpha / 2;
    return score;
}

static QString combineCandidateChunks(QStringList chunks)
{
    QSet<QString> seen;
    QStringList filtered;
    for (const QString &chunk : std::as_const(chunks))
    {
        const QString trimmed = chunk.trimmed();
        if (!looksLikeDocumentText(trimmed)) continue;
        if (seen.contains(trimmed)) continue;
        seen.insert(trimmed);
        filtered << trimmed;
    }
    if (filtered.size() > 1)
    {
        QVector<int> scores;
        scores.reserve(filtered.size());
        for (const QString &chunk : std::as_const(filtered))
        {
            const int s = chunkScore(chunk);
            scores << s;
        }
        QStringList prioritized;
        for (int i = 0; i < filtered.size(); ++i)
        {
            if (scores.at(i) > 0) prioritized << filtered.at(i);
        }
        if (!prioritized.isEmpty()) filtered = prioritized;
    }
    return filtered.join(QStringLiteral("\n"));
}

static QString extractUtf16Text(const QByteArray &data)
{
    if (data.isEmpty()) return {};

    QStringList chunks;
    QString current;
    const auto flushChunk = [&]() {
        if (!current.isEmpty()) chunks << current;
        current.clear();
    };

    int offset = 0;
    const int size = data.size();
    bool reading = false;
    while (offset + 1 < size)
    {
        if (!reading && (offset & 1))
        {
            ++offset;
            continue;
        }
        const quint16 value = qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(data.constData() + offset));
        if (isLikelyPrintableWordChar(value))
        {
            reading = true;
            if (value == 0x000D)
                current.append(QChar('\n'));
            else if (value == 0x000A)
                current.append(QChar('\n'));
            else
                current.append(QChar(value));
            offset += 2;
        }
        else
        {
            if (reading)
            {
                if (current.size() >= 3) flushChunk();
                else current.clear();
            }
            reading = false;
            offset += 2;
        }
    }
    if (reading && current.size() >= 3) flushChunk();

    QString asciiAccumulator;
    const auto flushAscii = [&]() {
        if (asciiAccumulator.size() >= 3)
        {
            const QString trimmed = asciiAccumulator.trimmed();
            if (!trimmed.isEmpty()) chunks << trimmed;
        }
        asciiAccumulator.clear();
    };
    for (int i = 0; i < size; ++i)
    {
        const uchar ch = static_cast<uchar>(data.at(i));
        if (ch >= 32 && ch <= 126)
            asciiAccumulator.append(QChar(ch));
        else if (ch == '\r' || ch == '\n')
            asciiAccumulator.append(QChar('\n'));
        else
            flushAscii();
    }
    flushAscii();

    return combineCandidateChunks(chunks);
}

static QString extractEncodedText(const QByteArray &data, QTextCodec *codec)
{
    if (!codec || data.isEmpty()) return {};

    QStringList rawChunks;
    QByteArray current;
    const auto flushChunk = [&]() {
        if (current.size() < 4)
        {
            current.clear();
            return;
        }
        QString decoded = codec->toUnicode(current);
        if (!decoded.isEmpty()) rawChunks << decoded;
        current.clear();
    };

    for (int i = 0; i < data.size(); ++i)
    {
        const uchar ch = static_cast<uchar>(data.at(i));
        if (ch == '\r' || ch == '\n')
        {
            current.append('\n');
        }
        else if (ch == '\t')
        {
            current.append('\t');
        }
        else if (ch >= 0x20 || ch >= 0xA1)
        {
            current.append(char(ch));
        }
        else
        {
            flushChunk();
        }
    }
    flushChunk();

    return combineCandidateChunks(rawChunks);
}

static quint16 detectBiffCodePage(const QByteArray &stream)
{
    int offset = 0;
    while (offset + 4 <= stream.size())
    {
        const uchar *ptr = reinterpret_cast<const uchar *>(stream.constData() + offset);
        const quint16 id = qFromLittleEndian<quint16>(ptr);
        const quint16 size = qFromLittleEndian<quint16>(ptr + 2);
        offset += 4;
        if (offset + size > stream.size()) break;
        if (id == 0x0042 && size >= 2)
            return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(stream.constData() + offset));
        offset += size;
    }
    return 0;
}

static QTextCodec *codecForCodePage(quint16 codePage)
{
    switch (codePage)
    {
    case 932: return QTextCodec::codecForName("Shift-JIS");
    case 936: return QTextCodec::codecForName("GB18030");
    case 949: return QTextCodec::codecForName("CP949");
    case 950: return QTextCodec::codecForName("Big5");
    case 1200: return QTextCodec::codecForName("UTF-16LE");
    case 1250: return QTextCodec::codecForName("Windows-1250");
    case 1251: return QTextCodec::codecForName("Windows-1251");
    case 1252: return QTextCodec::codecForName("Windows-1252");
    case 1253: return QTextCodec::codecForName("Windows-1253");
    case 1254: return QTextCodec::codecForName("Windows-1254");
    case 1255: return QTextCodec::codecForName("Windows-1255");
    case 1256: return QTextCodec::codecForName("Windows-1256");
    case 1257: return QTextCodec::codecForName("Windows-1257");
    case 1258: return QTextCodec::codecForName("Windows-1258");
    case 65001: return QTextCodec::codecForName("UTF-8");
    default:
        break;
    }
    return nullptr;
}

static QString decodeWithCodePage(const QByteArray &data, quint16 codePage)
{
    QTextCodec *codec = codecForCodePage(codePage);
    if (!codec) return {};
    return extractEncodedText(data, codec);
}

static QString decodeWithCodecNames(const QByteArray &data, std::initializer_list<const char *> names)
{
    for (const char *name : names)
    {
        QTextCodec *codec = QTextCodec::codecForName(name);
        if (!codec) continue;
        const QString text = extractEncodedText(data, codec);
        if (!text.isEmpty()) return text;
    }
    return {};
}

static QString readWpsHeuristic(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QByteArray data = f.readAll();
    return extractUtf16Text(data);
}

QString readWpsText(const QString &path)
{
    const QString parsed = readWpsViaWordBinary(path);
    if (!parsed.isEmpty()) return parsed;
    return readWpsHeuristic(path);
}

static QString formatMarkdownList(const QString &text)
{
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (lines.isEmpty()) return {};
    QStringList formatted;
    formatted.reserve(lines.size());
    for (const QString &line : lines)
    {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;
        formatted << QStringLiteral("- %1").arg(trimmed);
    }
    return formatted.join(QStringLiteral("\n"));
}

static bool isPptTextRecordType(quint16 type)
{
    if (type >= 0x0FA0 && type <= 0x0FA9) return true;
    switch (type)
    {
    case 0x0FBA:
    case 0x0FBB:
    case 0x0FBC:
    case 0x0D45:
        return true;
    default:
        break;
    }
    return false;
}

static bool looksUtf16TextPayload(const QByteArray &payload)
{
    if (payload.size() < 4 || (payload.size() & 1)) return false;
    const int charCount = payload.size() / 2;
    if (charCount == 0) return false;
    int printable = 0;
    for (int i = 0; i + 1 < payload.size(); i += 2)
    {
        const quint16 value = qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(payload.constData() + i));
        if (value == 0x0000) continue;
        if (value == 0x000A || value == 0x000D || value == 0x0009)
        {
            ++printable;
            continue;
        }
        if (isLikelyPrintableWordChar(value)) ++printable;
    }
    return printable * 2 >= charCount;
}

static bool looksLatinTextPayload(const QByteArray &payload)
{
    if (payload.size() < 3) return false;
    int printable = 0;
    for (char ch : payload)
    {
        const uchar value = static_cast<uchar>(ch);
        if (value >= 32 && value <= 126)
            ++printable;
        else if (value == '\r' || value == '\n' || value == '\t')
            ++printable;
    }
    return printable * 2 >= payload.size();
}

static QString decodePptTextRecord(quint16 type, const QByteArray &payload)
{
    if (!isPptTextRecordType(type) || payload.isEmpty()) return {};
    QString decoded;
    if (looksUtf16TextPayload(payload))
        decoded = QString::fromUtf16(reinterpret_cast<const ushort *>(payload.constData()), payload.size() / 2);
    else if (looksLatinTextPayload(payload))
        decoded = QString::fromLatin1(payload);
    else
        return {};

    QString cleaned;
    cleaned.reserve(decoded.size());
    for (QChar ch : std::as_const(decoded))
    {
        const ushort code = ch.unicode();
        if (code == 0) continue;
        if (code == 0x000D || code == 0x000B)
            cleaned.append(QChar('\n'));
        else if (code == 0x0009)
            cleaned.append(QChar(' '));
        else if (code >= 0x20)
            cleaned.append(ch);
    }
    return cleaned.trimmed();
}

static void collectPptTextRecords(const QByteArray &stream, int offset, int length, QStringList &out, QSet<QString> &seen)
{
    const int limit = qMin(offset + length, stream.size());
    int pos = offset;
    while (pos + 8 <= limit)
    {
        const quint16 verInst = qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(stream.constData() + pos));
        const quint16 recType = qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(stream.constData() + pos + 2));
        const quint32 recLen = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(stream.constData() + pos + 4));
        const int bodyStart = pos + 8;
        const int bodyEnd = bodyStart + static_cast<int>(recLen);
        if (bodyEnd > limit || bodyEnd > stream.size()) break;
        const quint16 recVer = verInst & 0x000F;
        if (recVer == 0x000F && recLen > 0)
        {
            collectPptTextRecords(stream, bodyStart, static_cast<int>(recLen), out, seen);
        }
        else if (recLen > 0)
        {
            const QByteArray payload = stream.mid(bodyStart, static_cast<int>(recLen));
            const QString decoded = decodePptTextRecord(recType, payload);
            if (!decoded.isEmpty())
            {
                const QStringList lines = decoded.split(QRegularExpression(QStringLiteral("[\\n]+")), Qt::SkipEmptyParts);
                for (const QString &line : lines)
                {
                    const QString trimmed = line.trimmed();
                    if (trimmed.isEmpty()) continue;
                    if (!looksLikeDocumentText(trimmed)) continue;
                    if (seen.contains(trimmed)) continue;
                    seen.insert(trimmed);
                    out << trimmed;
                }
            }
        }
        pos = bodyEnd;
    }
}

static QString readDpsViaPptBinary(const QString &path)
{
    CompoundFileReader reader;
    if (!reader.load(path)) return {};
    const QByteArray stream = reader.streamByName(QStringLiteral("PowerPoint Document"));
    if (stream.isEmpty()) return {};

    QStringList collected;
    QSet<QString> seen;
    collectPptTextRecords(stream, 0, stream.size(), collected, seen);
    if (collected.isEmpty()) return {};
    return collected.join(QStringLiteral("\n"));
}

struct XlsWorkbookCloser
{
    void operator()(xls::xlsWorkBook *wb) const
    {
        if (wb) xls::xls_close_WB(wb);
    }
};

struct XlsWorksheetCloser
{
    void operator()(xls::xlsWorkSheet *ws) const
    {
        if (ws) xls::xls_close_WS(ws);
    }
};

static QString xlsCellToString(const xls::xlsCell *cell)
{
    if (!cell) return {};
    if (cell->str) return QString::fromUtf8(cell->str);

    switch (cell->id)
    {
    case XLS_RECORD_BOOLERR:
        return cell->d != 0.0 ? QStringLiteral("TRUE") : QStringLiteral("FALSE");
    case XLS_RECORD_NUMBER:
    case XLS_RECORD_RK:
    case XLS_RECORD_FORMULA:
    case XLS_RECORD_FORMULA_ALT:
        if (std::isfinite(cell->d))
            return QString::number(cell->d, 'g', 15);
        break;
    default:
        break;
    }
    if (cell->l != 0) return QString::number(cell->l);
    return {};
}

static QString readEtViaLibxls(const QString &path)
{
    const QByteArray encoded = QFile::encodeName(path);
    xls::xls_error_t err = xls::LIBXLS_OK;
    std::unique_ptr<xls::xlsWorkBook, XlsWorkbookCloser> wb(xls::xls_open_file(encoded.constData(), "UTF-8", &err));
    if (!wb) return {};

    QStringList sections;
    int sheetIndex = 1;
    for (xls::DWORD i = 0; i < wb->sheets.count; ++i)
    {
        std::unique_ptr<xls::xlsWorkSheet, XlsWorksheetCloser> ws(xls::xls_getWorkSheet(wb.get(), static_cast<int>(i)));
        if (!ws) continue;
        if (xls::xls_parseWorkSheet(ws.get()) != xls::LIBXLS_OK) continue;

        const int lastRow = ws->rows.lastrow;
        const int lastCol = ws->rows.lastcol;
        if (lastRow < 0 || lastCol < 0) continue;

        QList<QStringList> rows;
        rows.reserve(lastRow + 1);
        int maxColumns = 0;
        for (int rowIndex = 0; rowIndex <= lastRow; ++rowIndex)
        {
            xls::xlsRow *row = xls::xls_row(ws.get(), static_cast<xls::WORD>(rowIndex));
            if (!row) continue;

            QStringList rowCells;
            rowCells.reserve(lastCol + 1);
            bool hasContent = false;
            for (int colIndex = 0; colIndex <= lastCol; ++colIndex)
            {
                xls::xlsCell *cell = xls::xls_cell(ws.get(), static_cast<xls::WORD>(rowIndex), static_cast<xls::WORD>(colIndex));
                const QString value = xlsCellToString(cell);
                if (!value.trimmed().isEmpty()) hasContent = true;
                rowCells << value;
            }
            while (!rowCells.isEmpty() && rowCells.last().trimmed().isEmpty()) rowCells.removeLast();
            if (!hasContent || rowCells.isEmpty()) continue;
            maxColumns = std::max(maxColumns, rowCells.size());
            rows.append(rowCells);
        }

        if (rows.isEmpty() || maxColumns == 0) continue;
        for (QStringList &row : rows)
        {
            while (row.size() < maxColumns) row << QString();
        }
        const QString table = makeMarkdownTable(rows);
        if (table.isEmpty()) continue;
        sections << QStringLiteral("## Sheet %1\n\n%2").arg(sheetIndex++).arg(table);
    }
    return sections.join(QStringLiteral("\n\n"));
}

QString readEtText(const QString &path)
{
    const QString viaLibxls = readEtViaLibxls(path);
    if (!viaLibxls.isEmpty()) return viaLibxls;

    CompoundFileReader reader;
    QByteArray workbook;
    if (reader.load(path))
        workbook = reader.streamByName(QStringLiteral("Workbook"));
    QString text;
    if (!workbook.isEmpty())
    {
        const quint16 codePage = detectBiffCodePage(workbook);
        if (codePage != 0 && codePage != 1200)
            text = decodeWithCodePage(workbook, codePage);
        if (text.isEmpty())
            text = extractUtf16Text(workbook);
    }
    if (text.isEmpty()) text = readWpsHeuristic(path);
    return text;
}

QString readDpsText(const QString &path)
{
    QString text = readDpsViaPptBinary(path);
    if (text.isEmpty())
    {
        CompoundFileReader reader;
        if (reader.load(path))
        {
            const QByteArray stream = reader.streamByName(QStringLiteral("PowerPoint Document"));
            if (!stream.isEmpty())
            {
                text = extractUtf16Text(stream);
                if (text.isEmpty())
                    text = decodeWithCodecNames(stream, {"GB18030", "Big5", "Shift-JIS", "Windows-1252"});
            }
        }
        if (text.isEmpty()) text = readWpsHeuristic(path);
    }
    if (text.isEmpty()) return {};
    const QString list = formatMarkdownList(text);
    if (list.isEmpty()) return text;
    return QStringLiteral("## DPS Slides\n\n%1").arg(list);
}

static QString readInlineStringElement(QXmlStreamReader &xr)
{
    QString text;
    while (!xr.atEnd())
    {
        auto token = xr.readNext();
        if (token == QXmlStreamReader::StartElement && xr.name() == QLatin1String("t"))
        {
            text += xr.readElementText(QXmlStreamReader::IncludeChildElements);
        }
        else if (token == QXmlStreamReader::EndElement && xr.name() == QLatin1String("is"))
        {
            break;
        }
    }
    return text;
}

static QString readSharedStringEntry(QXmlStreamReader &xr)
{
    QString text;
    while (!xr.atEnd())
    {
        auto token = xr.readNext();
        if (token == QXmlStreamReader::StartElement)
        {
            if (xr.name() == QLatin1String("t"))
                text += xr.readElementText(QXmlStreamReader::IncludeChildElements);
        }
        else if (token == QXmlStreamReader::EndElement && xr.name() == QLatin1String("si"))
        {
            break;
        }
    }
    return text;
}

static QVector<QString> parseSharedStringsXml(const QByteArray &xml)
{
    QVector<QString> strings;
    if (xml.isEmpty()) return strings;
    QXmlStreamReader xr(xml);
    while (!xr.atEnd())
    {
        auto token = xr.readNext();
        if (token == QXmlStreamReader::StartElement && xr.name() == QLatin1String("si"))
        {
            strings.append(readSharedStringEntry(xr));
        }
    }
    return strings;
}

static int extractTrailingNumber(const QString &name)
{
    int i = name.size() - 1;
    while (i >= 0 && !name.at(i).isDigit()) --i;
    if (i < 0) return 0;
    const int end = i;
    while (i >= 0 && name.at(i).isDigit()) --i;
    return name.mid(i + 1, end - i).toInt();
}

static void sortFilesByNumericSuffix(QStringList &names)
{
    std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
        const int na = extractTrailingNumber(a);
        const int nb = extractTrailingNumber(b);
        if (na == nb) return a < b;
        return na < nb;
    });
}

static QString readXlsxCell(QXmlStreamReader &xr, const QVector<QString> &sharedStrings)
{
    const QXmlStreamAttributes attrs = xr.attributes();
    const QString type = attrs.value(QStringLiteral("t")).toString();
    QString result;
    while (!xr.atEnd())
    {
        auto token = xr.readNext();
        if (token == QXmlStreamReader::StartElement)
        {
            if (xr.name() == QLatin1String("v"))
            {
                QString value = xr.readElementText(QXmlStreamReader::IncludeChildElements);
                if (type == QLatin1String("s"))
                {
                    bool ok = false;
                    const int idx = value.toInt(&ok);
                    if (ok && idx >= 0 && idx < sharedStrings.size())
                        result = sharedStrings.at(idx);
                    else
                        result = value;
                }
                else
                {
                    result = value;
                }
            }
            else if (xr.name() == QLatin1String("is"))
            {
                result = readInlineStringElement(xr);
            }
            else if (xr.name() == QLatin1String("t") && type == QLatin1String("inlineStr"))
            {
                result = xr.readElementText(QXmlStreamReader::IncludeChildElements);
            }
        }
        else if (token == QXmlStreamReader::EndElement && xr.name() == QLatin1String("c"))
        {
            break;
        }
    }
    return result.trimmed();
}

static QList<QStringList> collectWorksheetRows(const QByteArray &xml, const QVector<QString> &sharedStrings)
{
    QList<QStringList> rows;
    QStringList currentRow;
    bool inRow = false;
    QXmlStreamReader xr(xml);
    while (!xr.atEnd())
    {
        auto token = xr.readNext();
        if (token == QXmlStreamReader::StartElement)
        {
            if (xr.name() == QLatin1String("row"))
            {
                currentRow.clear();
                inRow = true;
            }
            else if (xr.name() == QLatin1String("c") && inRow)
            {
                currentRow << readXlsxCell(xr, sharedStrings);
            }
        }
        else if (token == QXmlStreamReader::EndElement && xr.name() == QLatin1String("row"))
        {
            inRow = false;
            if (!currentRow.isEmpty()) rows.append(currentRow);
        }
    }
    return rows;
}

QString readXlsxText(const QString &path)
{
    ZipArchiveReader zip;
    if (!zip.open(path)) return {};
    const QVector<QString> sharedStrings = parseSharedStringsXml(zip.fileData(QStringLiteral("xl/sharedStrings.xml")));
    QStringList sheetFiles = zip.filesWithPrefix(QStringLiteral("xl/worksheets/sheet"));
    if (sheetFiles.isEmpty()) return {};
    sortFilesByNumericSuffix(sheetFiles);

    QStringList sheets;
    int sheetIndex = 1;
    for (const QString &sheet : sheetFiles)
    {
        const QByteArray xml = zip.fileData(sheet);
        if (xml.isEmpty()) continue;
        const QList<QStringList> rows = collectWorksheetRows(xml, sharedStrings);
        if (rows.isEmpty()) continue;
        QString table = makeMarkdownTable(rows);
        if (table.isEmpty()) continue;
        sheets << QStringLiteral("## Sheet %1\n\n%2").arg(sheetIndex++).arg(table);
    }
    return sheets.join(QStringLiteral("\n\n"));
}

static QString parsePptSlideXml(const QByteArray &xml)
{
    QStringList paragraphs;
    QString current;
    QXmlStreamReader xr(xml);
    while (!xr.atEnd())
    {
        auto token = xr.readNext();
        if (token == QXmlStreamReader::StartElement)
        {
            if (xr.name() == QLatin1String("p"))
            {
                current.clear();
            }
            else if (xr.name() == QLatin1String("t"))
            {
                current += xr.readElementText(QXmlStreamReader::IncludeChildElements);
            }
            else if (xr.name() == QLatin1String("br"))
            {
                current += QLatin1Char('\n');
            }
        }
        else if (token == QXmlStreamReader::EndElement && xr.name() == QLatin1String("p"))
        {
            const QString trimmed = current.trimmed();
            if (!trimmed.isEmpty()) paragraphs << QStringLiteral("- ") + trimmed;
        }
    }
    return paragraphs.join(QStringLiteral("\n"));
}

QString readPptxText(const QString &path)
{
    ZipArchiveReader zip;
    if (!zip.open(path)) return {};
    QStringList slideFiles = zip.filesWithPrefix(QStringLiteral("ppt/slides/slide"));
    if (slideFiles.isEmpty()) return {};
    sortFilesByNumericSuffix(slideFiles);

    QStringList slides;
    int slideIndex = 1;
    for (const QString &slide : slideFiles)
    {
        const QByteArray xml = zip.fileData(slide);
        if (xml.isEmpty()) continue;
        const QString slideText = parsePptSlideXml(xml);
        if (slideText.trimmed().isEmpty()) continue;
        slides << QStringLiteral("## Slide %1\n\n%2").arg(slideIndex++).arg(slideText);
    }
    return slides.join(QStringLiteral("\n\n"));
}

namespace detail
{
QString extractEncodedTextForTest(const QByteArray &data, const QByteArray &codecName)
{
    QTextCodec *codec = QTextCodec::codecForName(codecName);
    return extractEncodedText(data, codec);
}

quint16 detectBiffCodePageForTest(const QByteArray &stream)
{
    return detectBiffCodePage(stream);
}
} // namespace detail

} // namespace DocParser
