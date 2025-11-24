#include "doc2md/detail_parsers.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <codecvt>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <iomanip>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

extern "C"
{
#include "miniz.h"
}

#include "tinyxml2.h"
#include "xls.h"

namespace doc2md
{
namespace detail
{
using ByteBuffer = std::vector<uint8_t>;

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string trim(const std::string &value)
{
    const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    auto begin = std::find_if_not(value.begin(), value.end(), isSpace);
    auto end = std::find_if_not(value.rbegin(), value.rend(), isSpace).base();
    if (begin >= end) return {};
    return std::string(begin, end);
}

static void replaceAll(std::string &text, const std::string &from, const std::string &to)
{
    if (from.empty()) return;
    std::string::size_type pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos)
    {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
}

static std::vector<std::string> splitLines(const std::string &text)
{
    std::vector<std::string> lines;
    std::string current;
    for (char ch : text)
    {
        if (ch == '\r') continue;
        if (ch == '\n')
        {
            lines.push_back(current);
            current.clear();
        }
        else
        {
            current.push_back(ch);
        }
    }
    lines.push_back(current);
    return lines;
}

static std::string join(const std::vector<std::string> &items, const std::string &separator)
{
    if (items.empty()) return {};
    std::ostringstream oss;
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (i) oss << separator;
        oss << items[i];
    }
    return oss.str();
}

static std::string escapeMarkdownCell(std::string text)
{
    replaceAll(text, "|", "\\|");
    replaceAll(text, "\r", "");
    replaceAll(text, "\n", "<br>");
    return trim(text);
}

static std::string makeMarkdownTable(const std::vector<std::vector<std::string>> &rows)
{
    if (rows.empty()) return {};
    size_t maxColumns = 0;
    for (const auto &row : rows)
        maxColumns = std::max(maxColumns, row.size());
    if (maxColumns == 0) return {};

    std::ostringstream oss;
    auto writeRow = [&](const std::vector<std::string> &row) {
        oss << "| ";
        for (size_t i = 0; i < maxColumns; ++i)
        {
            if (i) oss << " | ";
            if (i < row.size())
                oss << escapeMarkdownCell(row[i]);
            else
                oss << "";
        }
        oss << " |\n";
    };

    writeRow(rows.front());
    oss << "| ";
    for (size_t i = 0; i < maxColumns; ++i)
    {
        if (i) oss << " | ";
        oss << "---";
    }
    oss << " |\n";
    for (size_t i = 1; i < rows.size(); ++i)
        writeRow(rows[i]);
    std::string table = oss.str();
    if (!table.empty() && table.back() == '\n') table.pop_back();
    return table;
}

static std::string readBinaryFile(const std::string &path)
{
    std::ifstream stream(path.c_str(), std::ios::binary);
    if (!stream) return {};
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::string readTextFile(const std::string &path)
{
    std::ifstream stream(path.c_str(), std::ios::binary);
    if (!stream) return {};
    std::ostringstream oss;
    oss << stream.rdbuf();
    return oss.str();
}

static void appendUtf8CodePoint(std::string &out, uint32_t codePoint)
{
    if (codePoint <= 0x7F)
    {
        out.push_back(static_cast<char>(codePoint));
    }
    else if (codePoint <= 0x7FF)
    {
        out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
    else if (codePoint <= 0xFFFF)
    {
        out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
    else
    {
        out.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

static std::string utf16ToUtf8(const char16_t *data, size_t length)
{
    static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    try
    {
        return converter.to_bytes(data, data + length);
    }
    catch (const std::range_error &)
    {
        std::string out;
        out.reserve(length * 3);
        for (size_t i = 0; i < length; ++i)
        {
            const uint16_t lead = data[i];
            if (lead >= 0xD800 && lead <= 0xDBFF)
            {
                if (i + 1 < length)
                {
                    const uint16_t trail = data[i + 1];
                    if (trail >= 0xDC00 && trail <= 0xDFFF)
                    {
                        const uint32_t codePoint = 0x10000 + (((lead - 0xD800) << 10) | (trail - 0xDC00));
                        appendUtf8CodePoint(out, codePoint);
                        ++i;
                        continue;
                    }
                }
                // malformed lead surrogate; skip
                continue;
            }
            if (lead >= 0xDC00 && lead <= 0xDFFF)
            {
                // stray trail surrogate; skip
                continue;
            }
            appendUtf8CodePoint(out, lead);
        }
        return out;
    }
}

static std::string utf16ToUtf8(const std::u16string &str)
{
    return utf16ToUtf8(str.data(), str.size());
}

static std::string latin1ToUtf8(const std::string &text)
{
    std::string out;
    out.reserve(text.size() * 2);
    for (unsigned char ch : text)
    {
        if (ch < 0x80)
            out.push_back(static_cast<char>(ch));
        else
        {
            out.push_back(static_cast<char>(0xC0 | (ch >> 6)));
            out.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        }
    }
    return out;
}

class ZipArchive
{
public:
    ZipArchive() { std::memset(&m_archive, 0, sizeof(m_archive)); }
    ~ZipArchive() { close(); }

    bool open(const std::string &path)
    {
        close();
        const std::string bytes = readBinaryFile(path);
        if (bytes.empty()) return false;
        m_storage.assign(bytes.begin(), bytes.end());
        if (!mz_zip_reader_init_mem(&m_archive, m_storage.data(), m_storage.size(), 0))
        {
            close();
            return false;
        }
        m_open = true;
        return true;
    }

    std::string fileContent(const std::string &name) const
    {
        if (!m_open) return {};
        const int index = mz_zip_reader_locate_file(const_cast<mz_zip_archive *>(&m_archive), name.c_str(), nullptr, 0);
        if (index < 0) return {};
        size_t outSize = 0;
        void *ptr = mz_zip_reader_extract_to_heap(const_cast<mz_zip_archive *>(&m_archive), index, &outSize, 0);
        if (!ptr || outSize == 0)
        {
            if (ptr) mz_free(ptr);
            return {};
        }
        std::string data(static_cast<const char *>(ptr), static_cast<size_t>(outSize));
        mz_free(ptr);
        return data;
    }

    std::vector<std::string> filesWithPrefix(const std::string &prefix) const
    {
        std::vector<std::string> names;
        if (!m_open) return names;
        const int count = static_cast<int>(mz_zip_reader_get_num_files(&m_archive));
        for (int i = 0; i < count; ++i)
        {
            mz_zip_archive_file_stat statRecord;
            if (!mz_zip_reader_file_stat(const_cast<mz_zip_archive *>(&m_archive), i, &statRecord))
                continue;
            std::string name = statRecord.m_filename ? statRecord.m_filename : "";
            if (name.compare(0, prefix.size(), prefix) == 0) names.push_back(name);
        }
        std::sort(names.begin(), names.end());
        return names;
    }

private:
    void close()
    {
        if (m_open)
        {
            mz_zip_reader_end(&m_archive);
            m_open = false;
        }
        m_storage.clear();
    }

    mutable mz_zip_archive m_archive;
    ByteBuffer m_storage;
    bool m_open = false;
};

constexpr uint32_t OleFreeSector = 0xFFFFFFFFu;
constexpr uint32_t OleEndOfChain = 0xFFFFFFFEu;

struct OleDirectoryEntry
{
    std::string name;
    uint8_t type = 0;
    uint32_t startSector = OleEndOfChain;
    uint64_t size = 0;
};

class CompoundFileReader
{
public:
    bool load(const std::string &path);
    std::string streamByName(const std::string &name) const;

private:
    int sectorSize() const { return 1 << m_sectorShift; }
    int miniSectorSize() const { return 1 << m_miniSectorShift; }
    int64_t sectorOffset(uint32_t sector) const;
    std::string sectorData(uint32_t sector) const;
    bool appendDifatSector(uint32_t sector, std::vector<uint32_t> &difat);
    bool buildFat(const std::vector<uint32_t> &difat);
    bool buildMiniFat(uint32_t startSector, uint32_t sectorCount);
    bool buildDirectory();
    bool buildMiniStream();
    std::string readStream(uint32_t startSector, uint64_t size, bool useMini) const;
    uint32_t nextSector(uint32_t current) const;

    ByteBuffer m_data;
    std::vector<uint32_t> m_fat;
    std::vector<uint32_t> m_miniFat;
    std::vector<OleDirectoryEntry> m_entries;
    ByteBuffer m_miniStream;
    uint16_t m_sectorShift = 9;
    uint16_t m_miniSectorShift = 6;
    uint32_t m_miniStreamCutoff = 4096;
    uint32_t m_firstDirSector = 0;
    uint32_t m_firstMiniFatSector = OleEndOfChain;
    uint32_t m_numMiniFatSectors = 0;
    uint16_t m_majorVersion = 3;
    bool m_valid = false;
};

template <typename T>
static T readLe(const uint8_t *ptr)
{
    T value = 0;
    std::memcpy(&value, ptr, sizeof(T));
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    uint8_t *raw = reinterpret_cast<uint8_t *>(&value);
    std::reverse(raw, raw + sizeof(T));
#endif
    return value;
}

bool CompoundFileReader::load(const std::string &path)
{
    m_valid = false;
    const std::string bytes = readBinaryFile(path);
    if (bytes.size() < 512) return false;
    m_data.assign(bytes.begin(), bytes.end());

    const uint8_t *header = m_data.data();
    const uint64_t signature = readLe<uint64_t>(header);
    if (signature != 0xE11AB1A1E011CFD0ULL) return false;

    m_majorVersion = readLe<uint16_t>(header + 0x1A);
    const uint16_t byteOrder = readLe<uint16_t>(header + 0x1C);
    if (byteOrder != 0xFFFE) return false;
    m_sectorShift = readLe<uint16_t>(header + 0x1E);
    m_miniSectorShift = readLe<uint16_t>(header + 0x20);
    m_miniStreamCutoff = readLe<uint32_t>(header + 0x38);
    m_firstDirSector = readLe<uint32_t>(header + 0x30);
    m_firstMiniFatSector = readLe<uint32_t>(header + 0x3C);
    m_numMiniFatSectors = readLe<uint32_t>(header + 0x40);
    uint32_t firstDifatSector = readLe<uint32_t>(header + 0x44);
    uint32_t numDifatSectors = readLe<uint32_t>(header + 0x48);

    std::vector<uint32_t> difat;
    difat.reserve(128);
    const uint8_t *difatHead = header + 0x4C;
    for (int i = 0; i < 109; ++i)
    {
        uint32_t entry = readLe<uint32_t>(difatHead + i * 4);
        if (entry != OleFreeSector) difat.push_back(entry);
    }
    uint32_t difatSector = firstDifatSector;
    while (numDifatSectors > 0 && difatSector != OleEndOfChain)
    {
        if (!appendDifatSector(difatSector, difat)) return false;
        const std::string block = sectorData(difatSector);
        if (block.size() != static_cast<size_t>(sectorSize())) return false;
        difatSector = readLe<uint32_t>(reinterpret_cast<const uint8_t *>(block.data()) + sectorSize() - 4);
        --numDifatSectors;
    }
    if (!buildFat(difat)) return false;
    if (!buildMiniFat(m_firstMiniFatSector, m_numMiniFatSectors)) return false;
    if (!buildDirectory()) return false;
    if (!buildMiniStream()) return false;
    m_valid = true;
    return true;
}

std::string CompoundFileReader::streamByName(const std::string &name) const
{
    if (!m_valid) return {};
    std::string lower = toLower(name);
    for (const auto &entry : m_entries)
    {
        if (entry.type != 2) continue;
        if (toLower(entry.name) != lower) continue;
        const bool useMini = entry.size < m_miniStreamCutoff && !m_miniStream.empty();
        return readStream(entry.startSector, entry.size, useMini);
    }
    return {};
}

int64_t CompoundFileReader::sectorOffset(uint32_t sector) const
{
    if (sector == OleEndOfChain) return -1;
    const int64_t offset = 512 + static_cast<int64_t>(sector) * sectorSize();
    if (offset < 0 || offset + sectorSize() > static_cast<int64_t>(m_data.size())) return -1;
    return offset;
}

std::string CompoundFileReader::sectorData(uint32_t sector) const
{
    const int64_t offset = sectorOffset(sector);
    if (offset < 0) return {};
    return std::string(reinterpret_cast<const char *>(m_data.data()) + offset, sectorSize());
}

bool CompoundFileReader::appendDifatSector(uint32_t sector, std::vector<uint32_t> &difat)
{
    const std::string block = sectorData(sector);
    if (block.size() != static_cast<size_t>(sectorSize())) return false;
    const int intsPerSector = sectorSize() / 4;
    const uint8_t *ptr = reinterpret_cast<const uint8_t *>(block.data());
    for (int i = 0; i < intsPerSector - 1; ++i)
    {
        uint32_t value = readLe<uint32_t>(ptr + i * 4);
        if (value != OleFreeSector) difat.push_back(value);
    }
    return true;
}

bool CompoundFileReader::buildFat(const std::vector<uint32_t> &difat)
{
    if (difat.empty()) return false;
    m_fat.clear();
    const int intsPerSector = sectorSize() / 4;
    for (uint32_t sector : difat)
    {
        const std::string block = sectorData(sector);
        if (block.size() != static_cast<size_t>(sectorSize())) return false;
        const uint8_t *ptr = reinterpret_cast<const uint8_t *>(block.data());
        for (int i = 0; i < intsPerSector; ++i)
            m_fat.push_back(readLe<uint32_t>(ptr + i * 4));
    }
    return !m_fat.empty();
}

bool CompoundFileReader::buildMiniFat(uint32_t startSector, uint32_t sectorCount)
{
    m_miniFat.clear();
    if (startSector == OleEndOfChain || sectorCount == 0) return true;
    uint32_t sector = startSector;
    const int intsPerSector = sectorSize() / 4;
    for (uint32_t i = 0; i < sectorCount && sector != OleEndOfChain; ++i)
    {
        const std::string block = sectorData(sector);
        if (block.size() != static_cast<size_t>(sectorSize())) return false;
        const uint8_t *ptr = reinterpret_cast<const uint8_t *>(block.data());
        for (int j = 0; j < intsPerSector; ++j)
            m_miniFat.push_back(readLe<uint32_t>(ptr + j * 4));
        sector = nextSector(sector);
    }
    return true;
}

bool CompoundFileReader::buildDirectory()
{
    const std::string dirStream = readStream(m_firstDirSector, 0, false);
    if (dirStream.empty()) return false;
    m_entries.clear();
    const int entrySize = 128;
    const int count = static_cast<int>(dirStream.size()) / entrySize;
    for (int i = 0; i < count; ++i)
    {
        const uint8_t *base = reinterpret_cast<const uint8_t *>(dirStream.data()) + i * entrySize;
        const uint16_t nameLen = readLe<uint16_t>(base + 64);
        if (nameLen < 2) continue;
        const int charCount = std::max(0, std::min<int>(32, nameLen / 2 - 1));
        std::u16string name;
        name.resize(charCount);
        std::memcpy(&name[0], base, charCount * sizeof(char16_t));
        OleDirectoryEntry entry;
        entry.name = utf16ToUtf8(name);
        entry.type = static_cast<uint8_t>(base[66]);
        entry.startSector = readLe<uint32_t>(base + 116);
        if (m_majorVersion >= 4)
            entry.size = readLe<uint64_t>(base + 120);
        else
            entry.size = readLe<uint32_t>(base + 120);
        m_entries.push_back(entry);
    }
    return !m_entries.empty();
}

bool CompoundFileReader::buildMiniStream()
{
    auto it = std::find_if(m_entries.begin(), m_entries.end(), [](const OleDirectoryEntry &entry) { return entry.type == 5; });
    if (it == m_entries.end()) return false;
    const std::string root = readStream(it->startSector, it->size, false);
    m_miniStream.assign(root.begin(), root.end());
    return true;
}

std::string CompoundFileReader::readStream(uint32_t startSector, uint64_t size, bool useMini) const
{
    if (startSector == OleEndOfChain) return {};
    std::string buffer;
    if (useMini)
    {
        uint32_t sector = startSector;
        uint64_t remaining = size;
        const int miniSz = miniSectorSize();
        while (sector != OleEndOfChain && remaining > 0)
        {
            const int64_t offset = static_cast<int64_t>(sector) * miniSz;
            if (offset < 0 || offset + miniSz > static_cast<int64_t>(m_miniStream.size())) break;
            const int chunk = static_cast<int>(std::min<uint64_t>(remaining, miniSz));
            buffer.append(reinterpret_cast<const char *>(m_miniStream.data()) + offset, chunk);
            remaining -= chunk;
            if (sector >= m_miniFat.size()) break;
            sector = m_miniFat[sector];
        }
        if (size && buffer.size() > size) buffer.resize(static_cast<size_t>(size));
        return buffer;
    }

    uint32_t sector = startSector;
    uint64_t remaining = size;
    const int sectorSz = sectorSize();
    while (sector != OleEndOfChain && (remaining > 0 || size == 0))
    {
        const int64_t offset = sectorOffset(sector);
        if (offset < 0) break;
        const int chunk = static_cast<int>((size == 0) ? sectorSz : std::min<uint64_t>(remaining, sectorSz));
        buffer.append(reinterpret_cast<const char *>(m_data.data()) + offset, chunk);
        if (size)
        {
            remaining -= chunk;
            if (remaining == 0) break;
        }
        sector = nextSector(sector);
    }
    if (size && buffer.size() > size) buffer.resize(static_cast<size_t>(size));
    return buffer;
}

uint32_t CompoundFileReader::nextSector(uint32_t current) const
{
    if (current >= m_fat.size()) return OleEndOfChain;
    return m_fat[current];
}

std::string markdownToText(const std::string &markdown)
{
    std::string md = markdown;
    std::regex fenced("```[\\s\\S]*?```", std::regex::optimize);
    md = std::regex_replace(md, fenced, "");
    md = std::regex_replace(md, std::regex("`([^`]*)`"), "$1");
    md = std::regex_replace(md, std::regex("!\\[([^\\]]*)\\]\\([^\\)]*\\)"), "$1");
    md = std::regex_replace(md, std::regex("\\[([^\\]]*)\\]\\([^\\)]*\\)"), "$1");
    md = std::regex_replace(md, std::regex("(?m)^\\s*#+\\s*"), "");
    md = std::regex_replace(md, std::regex("(?m)^>\\s*"), "");
    md = std::regex_replace(md, std::regex("(?m)^\\|.*\\|$"), "");
    md = std::regex_replace(md, std::regex("(?m)^\\s*\\|?\\s*:-*:?\\s*(\\|\\s*:-*:?\\s*)*$"), "");
    md = std::regex_replace(md, std::regex("<[^>]+>"), "");
    replaceAll(md, "**", "");
    replaceAll(md, "*", "");
    return md;
}

std::string htmlToText(const std::string &htmlInput)
{
    std::string html = htmlInput;
    html = std::regex_replace(html, std::regex("<style[\\s\\S]*?</style>", std::regex::icase), "");
    html = std::regex_replace(html, std::regex("<script[\\s\\S]*?</script>", std::regex::icase), "");
    html = std::regex_replace(html, std::regex("<br\\s*/?>", std::regex::icase), "\n");
    html = std::regex_replace(html, std::regex("</p>", std::regex::icase), "\n\n");
    html = std::regex_replace(html, std::regex("<[^>]+>"), "");
    replaceAll(html, "&nbsp;", " ");
    replaceAll(html, "&lt;", "<");
    replaceAll(html, "&gt;", ">");
    replaceAll(html, "&amp;", "&");
    return trim(html);
}

struct TextPiece
{
    uint32_t cpStart = 0;
    uint32_t cpEnd = 0;
    uint32_t fileOffset = 0;
    bool unicode = false;
};

struct FibInfo
{
    bool useTable1 = false;
    bool complex = false;
    uint32_t fcMin = 0;
    uint32_t fcMac = 0;
    uint32_t fcClx = 0;
    uint32_t lcbClx = 0;
};

static bool parseFib(const std::string &wordStream, FibInfo &info)
{
    if (wordStream.size() < 256) return false;
    const uint8_t *ptr = reinterpret_cast<const uint8_t *>(wordStream.data());
    if (readLe<uint16_t>(ptr) != 0xA5EC) return false;
    const uint16_t flags = readLe<uint16_t>(ptr + 0x0A);
    info.useTable1 = (flags & 0x0200) != 0;
    info.complex = (flags & 0x0004) != 0;
    info.fcMin = readLe<uint32_t>(ptr + 0x18);
    info.fcMac = readLe<uint32_t>(ptr + 0x1C);

    int pos = 32;
    if (wordStream.size() < pos + 2) return false;
    const uint16_t csw = readLe<uint16_t>(ptr + pos);
    pos += 2 + csw * 2;
    if (wordStream.size() < pos + 2) return false;
    const uint16_t cslw = readLe<uint16_t>(ptr + pos);
    pos += 2 + cslw * 4;
    if (wordStream.size() < pos + 2) return false;
    const uint16_t cbRgFcLcb = readLe<uint16_t>(ptr + pos);
    pos += 2;
    if (wordStream.size() < pos + cbRgFcLcb * 8) return false;
    const int idx = 33;
    if (cbRgFcLcb > idx)
    {
        const int offset = pos + idx * 8;
        info.fcClx = readLe<uint32_t>(ptr + offset);
        info.lcbClx = readLe<uint32_t>(ptr + offset + 4);
    }
    return true;
}

static std::vector<TextPiece> parseTextPieces(const std::string &tableStream, uint32_t fcClx, uint32_t lcbClx)
{
    std::vector<TextPiece> pieces;
    if (fcClx == 0 || lcbClx == 0) return pieces;
    if (fcClx + lcbClx > tableStream.size()) return pieces;
    const uint8_t *clx = reinterpret_cast<const uint8_t *>(tableStream.data() + fcClx);
    int pos = 0;
    while (pos < static_cast<int>(lcbClx))
    {
        const uint8_t clxt = clx[pos++];
        if (clxt == 0x01)
        {
            if (pos + 4 > static_cast<int>(lcbClx)) break;
            const uint32_t lcb = readLe<uint32_t>(clx + pos);
            pos += 4;
            if (lcb == 0 || pos + static_cast<int>(lcb) > static_cast<int>(lcbClx)) break;
            const uint8_t *plc = clx + pos;
            pos += static_cast<int>(lcb);
            const int pieceCount = (static_cast<int>(lcb) - 4) / (8 + 4);
            if (pieceCount <= 0) break;
            std::vector<uint32_t> cps(pieceCount + 1);
            for (size_t i = 0; i < cps.size(); ++i)
                cps[i] = readLe<uint32_t>(plc + i * 4);
            const uint8_t *pcd = plc + (pieceCount + 1) * 4;
            for (int i = 0; i < pieceCount; ++i)
            {
                const uint32_t fc = readLe<uint32_t>(pcd + i * 8 + 2);
                const bool unicode = (fc & 0x40000000u) == 0;
                const uint32_t fileOffset = unicode ? fc : (fc & 0x3FFFFFFFu) / 2;
                TextPiece piece;
                piece.cpStart = cps[i];
                piece.cpEnd = cps[i + 1];
                piece.fileOffset = fileOffset;
                piece.unicode = unicode;
                pieces.push_back(piece);
            }
            break;
        }
        else if (clxt == 0x02)
        {
            if (pos + 2 > static_cast<int>(lcbClx)) break;
            const uint16_t cb = readLe<uint16_t>(clx + pos);
            pos += 2 + cb;
        }
        else
        {
            break;
        }
    }
    return pieces;
}

static std::string decodePieces(const std::string &wordStream, const std::vector<TextPiece> &pieces)
{
    if (pieces.empty()) return {};
    std::string out;
    for (const TextPiece &piece : pieces)
    {
        if (piece.cpEnd <= piece.cpStart) continue;
        const uint32_t charCount = piece.cpEnd - piece.cpStart;
        const uint32_t byteCount = piece.unicode ? charCount * 2 : charCount;
        if (piece.fileOffset + byteCount > wordStream.size()) continue;
        if (piece.unicode)
        {
            const char16_t *src = reinterpret_cast<const char16_t *>(wordStream.data() + piece.fileOffset);
            out += utf16ToUtf8(src, charCount);
        }
        else
        {
            const std::string bytes(wordStream.data() + piece.fileOffset, byteCount);
            out += latin1ToUtf8(bytes);
        }
    }
    return out;
}

static std::string decodeSimpleRange(const std::string &wordStream, uint32_t fcMin, uint32_t fcMac)
{
    if (fcMac <= fcMin || fcMin >= wordStream.size()) return {};
    uint32_t limit = std::min<uint32_t>(fcMac, wordStream.size());
    uint32_t span = limit - fcMin;
    if (span < 4) return {};
    if (span % 2 != 0) --span;
    const char16_t *src = reinterpret_cast<const char16_t *>(wordStream.data() + fcMin);
    return utf16ToUtf8(src, span / 2);
}

static std::vector<std::string> splitTabLine(const std::string &line)
{
    std::vector<std::string> cells;
    std::string current;
    for (char ch : line)
    {
        if (ch == '\t')
        {
            cells.push_back(trim(current));
            current.clear();
        }
        else
        {
            current.push_back(ch);
        }
    }
    cells.push_back(trim(current));
    return cells;
}

static bool isTabularLine(const std::string &line, size_t &columnCount)
{
    if (line.find('\t') == std::string::npos) return false;
    const std::vector<std::string> cells = splitTabLine(line);
    if (cells.size() < 2) return false;
    size_t nonEmpty = 0;
    for (const std::string &cell : cells)
    {
        if (!cell.empty()) ++nonEmpty;
    }
    if (nonEmpty == 0) return false;
    columnCount = cells.size();
    return true;
}

static std::vector<std::vector<std::string>> expandFlattenedTabRows(const std::vector<std::string> &tokens)
{
    if (tokens.size() < 4) return {};
    size_t bestColumns = 0;
    double bestScore = 0.0;
    const size_t maxColumns = std::min<size_t>(32, tokens.size() / 2);
    for (size_t candidate = 2; candidate <= maxColumns; ++candidate)
    {
        if (tokens.size() % candidate != 0) continue;
        const size_t rows = tokens.size() / candidate;
        if (rows < 2) continue;
        double density = 0.0;
        for (size_t r = 0; r < rows; ++r)
        {
            size_t nonEmpty = 0;
            for (size_t c = 0; c < candidate; ++c)
            {
                if (!tokens[r * candidate + c].empty()) ++nonEmpty;
            }
            density += static_cast<double>(nonEmpty) / candidate;
        }
        density /= rows;
        if (density > bestScore + 1e-6)
        {
            bestScore = density;
            bestColumns = candidate;
        }
    }
    if (bestColumns == 0) return {};
    const size_t rows = tokens.size() / bestColumns;
    std::vector<std::vector<std::string>> expanded(rows);
    for (size_t r = 0; r < rows; ++r)
    {
        auto begin = tokens.begin() + static_cast<std::ptrdiff_t>(r * bestColumns);
        auto end = begin + static_cast<std::ptrdiff_t>(bestColumns);
        expanded[r] = std::vector<std::string>(begin, end);
    }
    return expanded;
}

static std::string convertLinesWithTables(const std::vector<std::string> &lines)
{
    if (lines.empty()) return {};
    std::vector<std::string> blocks;
    size_t index = 0;
    while (index < lines.size())
    {
        size_t columnCount = 0;
        if (isTabularLine(lines[index], columnCount))
        {
            std::vector<std::vector<std::string>> rows;
            std::vector<std::vector<std::string>> rawRowTokens;
            size_t maxColumns = columnCount;
            size_t cursor = index;
            while (cursor < lines.size())
            {
                size_t rowColumns = 0;
                if (!isTabularLine(lines[cursor], rowColumns)) break;
                std::vector<std::string> cells = splitTabLine(lines[cursor]);
                if (cells.empty()) break;
                maxColumns = std::max(maxColumns, cells.size());
                rawRowTokens.push_back(cells);
                rows.push_back(std::move(cells));
                ++cursor;
            }
            if (rows.size() < 2 && rows.size() == 1)
            {
                const std::vector<std::vector<std::string>> expanded = expandFlattenedTabRows(rawRowTokens.front());
                if (!expanded.empty())
                {
                    rows = expanded;
                    maxColumns = 0;
                    for (const auto &row : rows) maxColumns = std::max(maxColumns, row.size());
                }
            }
            if (rows.size() >= 2 && maxColumns >= 2)
            {
                for (auto &row : rows)
                    if (row.size() < maxColumns) row.resize(maxColumns);
                const std::string table = makeMarkdownTable(rows);
                if (!table.empty()) blocks.push_back(table);
            }
            else
            {
                blocks.push_back(lines[index]);
                cursor = index + 1;
            }
            index = cursor;
        }
        else
        {
            blocks.push_back(lines[index]);
            ++index;
        }
    }
    return join(blocks, "\n\n");
}

static bool hasFieldInstructionPrefix(const std::string &text, const char *keyword)
{
    const size_t len = std::char_traits<char>::length(keyword);
    if (text.size() <= len) return false;
    if (text.compare(0, len, keyword) != 0) return false;
    const unsigned char next = static_cast<unsigned char>(text[len]);
    return std::isspace(next) || next == '\\' || next == '"';
}

static bool looksLikeFieldInstruction(const std::string &text)
{
    static const std::array<const char *, 4> keywords = {"HYPERLINK", "INCLUDEPICTURE", "MERGEFIELD", "PAGEREF"};
    for (const char *keyword : keywords)
    {
        if (hasFieldInstructionPrefix(text, keyword)) return true;
    }
    return false;
}

static std::string normalizeWordText(const std::string &raw)
{
    if (raw.empty()) return {};
    std::string cleaned;
    cleaned.reserve(raw.size());
    std::vector<bool> fieldInstructionStack;
    int pendingInstructionFields = 0;
    const auto inFieldInstruction = [&]() {
        return pendingInstructionFields > 0;
    };
    for (size_t i = 0; i < raw.size(); ++i)
    {
        unsigned char byte = static_cast<unsigned char>(raw[i]);
        if (byte == 0x00) continue;
        if (byte == 0x13) // field begin
        {
            fieldInstructionStack.push_back(false);
            ++pendingInstructionFields;
            continue;
        }
        if (byte == 0x14) // field separator
        {
            if (!fieldInstructionStack.empty() && !fieldInstructionStack.back())
            {
                fieldInstructionStack.back() = true;
                if (pendingInstructionFields > 0) --pendingInstructionFields;
            }
            continue;
        }
        if (byte == 0x15) // field end
        {
            if (!fieldInstructionStack.empty())
            {
                if (!fieldInstructionStack.back() && pendingInstructionFields > 0) --pendingInstructionFields;
                fieldInstructionStack.pop_back();
            }
            continue;
        }
        if (inFieldInstruction()) continue;
        if (byte == 0x07)
        {
            size_t runLength = 0;
            while (i < raw.size() && static_cast<unsigned char>(raw[i]) == 0x07)
            {
                ++runLength;
                ++i;
            }
            if (runLength == 0) continue;
            if (runLength == 1)
            {
                cleaned.push_back('\t');
            }
            else
            {
                for (size_t j = 0; j + 1 < runLength; ++j)
                    cleaned.push_back('\t');
                cleaned.push_back('\n');
            }
            --i;
            continue;
        }
        if (byte == 0x0D || byte == 0x0B || byte == 0x0C || byte == 0x1E || byte == 0x1F)
        {
            cleaned.push_back('\n');
            continue;
        }
        if (byte < 0x20 && byte != 0x09)
            continue;
        cleaned.push_back(static_cast<char>(byte));
    }
    std::vector<std::string> lines = splitLines(cleaned);
    std::vector<std::string> filtered;
    filtered.reserve(lines.size());
    for (const std::string &line : lines)
    {
        const std::string trimmedLine = trim(line);
        if (!trimmedLine.empty()) filtered.push_back(trimmedLine);
    }
    return convertLinesWithTables(filtered);
}

static bool looksLikeDocumentText(const std::string &chunk)
{
    const std::string trimmedText = trim(chunk);
    if (trimmedText.size() < 2 || trimmedText.size() > 1024) return false;
    static const std::unordered_set<std::string> noise = {
        "Root Entry", "SummaryInformation", "DocumentSummaryInformation", "WordDocument",
        "0Table", "1Table", "Normal.dotm", "WpsCustomData", "KSOProductBuildVer",
        "KSOTemplateDocerSaveRecord"};
    if (noise.count(trimmedText)) return false;
    return true;
}

static bool isLikelyPrintableWordChar(uint16_t ch)
{
    if (ch == 0x0000 || ch == 0xFFFF || ch == 0xFFFE) return false;
    if (ch == 0x0009 || ch == 0x000A || ch == 0x000D) return true;
    if (ch == 0x3000) return true;
    if (ch >= 0x20 && ch <= 0xD7FF) return true;
    if (ch >= 0xE000 && ch <= 0xF8FF) return false;
    if (ch >= 0xF000) return false;
    return false;
}

static bool isValidUtf8(const std::string &text)
{
    int expected = 0;
    for (unsigned char byte : text)
    {
        if (expected == 0)
        {
            if ((byte & 0x80) == 0)
                continue;
            else if ((byte & 0xE0) == 0xC0)
                expected = 1;
            else if ((byte & 0xF0) == 0xE0)
                expected = 2;
            else if ((byte & 0xF8) == 0xF0)
                expected = 3;
            else
                return false;
        }
        else
        {
            if ((byte & 0xC0) != 0x80) return false;
            --expected;
        }
    }
    return expected == 0;
}

static int chunkScore(const std::string &chunk)
{
    int cjk = 0;
    int digits = 0;
    int asciiAlpha = 0;
    for (unsigned char ch : chunk)
    {
        if (ch >= 0x4E && ch <= 0x9F) ++cjk;
        if (std::isdigit(ch)) ++digits;
        if (std::isalpha(ch) && ch < 0x80) ++asciiAlpha;
    }
    int score = cjk * 5 + digits * 3 - asciiAlpha;
    if (digits >= 6 && digits >= cjk && digits > asciiAlpha) score += digits * 10;
    return score;
}

static std::string combineCandidateChunks(const std::vector<std::string> &chunks)
{
    if (chunks.empty()) return {};
    std::unordered_set<std::string> seen;
    std::vector<std::string> filtered;
    filtered.reserve(chunks.size());
    for (const std::string &chunk : chunks)
    {
        const std::string trimmedChunk = trim(chunk);
        if (trimmedChunk.empty() || !looksLikeDocumentText(trimmedChunk)) continue;
        if (looksLikeFieldInstruction(trimmedChunk)) continue;
        if (seen.insert(trimmedChunk).second) filtered.push_back(trimmedChunk);
    }
    if (filtered.size() > 1)
    {
        std::vector<int> scores(filtered.size());
        int bestScore = std::numeric_limits<int>::min();
        for (size_t i = 0; i < filtered.size(); ++i)
        {
            scores[i] = chunkScore(filtered[i]);
            bestScore = std::max(bestScore, scores[i]);
        }
        const int cutoff = bestScore > 0 ? bestScore - 4 : bestScore;
        std::vector<std::string> prioritized;
        prioritized.reserve(filtered.size());
        for (size_t i = 0; i < filtered.size(); ++i)
        {
            if (scores[i] >= cutoff && scores[i] > 0) prioritized.push_back(filtered[i]);
        }
        if (!prioritized.empty()) filtered.swap(prioritized);
    }
    return join(filtered, "\n");
}

static std::string extractUtf16Text(const std::string &data)
{
    if (data.empty()) return {};
    std::vector<std::string> chunks;
    std::u16string current;
    bool reading = false;
    const uint8_t *ptr = reinterpret_cast<const uint8_t *>(data.data());
    for (size_t offset = 0; offset + 1 < data.size(); offset += 2)
    {
        uint16_t value = readLe<uint16_t>(ptr + offset);
        if (value >= 0x20 && value != 0xFFFF && value != 0xFFFE)
        {
            reading = true;
            if (value == 0x000D || value == 0x000A)
                current.push_back(u'\n');
            else
                current.push_back(static_cast<char16_t>(value));
        }
        else if (reading)
        {
            if (current.size() >= 3) chunks.push_back(utf16ToUtf8(current));
            current.clear();
            reading = false;
        }
    }
    if (reading && current.size() >= 3) chunks.push_back(utf16ToUtf8(current));

    return combineCandidateChunks(chunks);
}

static uint16_t detectBiffCodePage(const std::string &stream)
{
    size_t offset = 0;
    while (offset + 4 <= stream.size())
    {
        const uint8_t *ptr = reinterpret_cast<const uint8_t *>(stream.data() + offset);
        const uint16_t id = readLe<uint16_t>(ptr);
        const uint16_t size = readLe<uint16_t>(ptr + 2);
        offset += 4;
        if (offset + size > stream.size()) break;
        if (id == 0x0042 && size >= 2)
            return readLe<uint16_t>(reinterpret_cast<const uint8_t *>(stream.data() + offset));
        offset += size;
    }
    return 0;
}

static std::string convertBufferWithCodePage(const std::string &buffer, uint32_t codePage)
{
    if (buffer.empty()) return {};
    if (codePage == 65001) return buffer;
#ifdef _WIN32
    const UINT cp = static_cast<UINT>(codePage);
    const int wideLen = MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS, buffer.data(), static_cast<int>(buffer.size()), nullptr, 0);
    if (wideLen <= 0) return {};
    std::wstring wide(wideLen, L'\0');
    if (MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS, buffer.data(), static_cast<int>(buffer.size()), &wide[0], wideLen) <= 0)
        return {};
    std::u16string u16;
    u16.reserve(wide.size());
    for (wchar_t ch : wide) u16.push_back(static_cast<char16_t>(ch));
    return utf16ToUtf8(u16);
#else
    (void)buffer;
    (void)codePage;
    return {};
#endif
}

static std::string decodeWithCodePage(const std::string &data, uint16_t codePage)
{
    if (data.empty()) return {};
    if (codePage == 1200) return extractUtf16Text(data);

    std::vector<std::string> decodedChunks;
    std::string current;
    const auto flushChunk = [&]() {
        if (current.size() < 4)
        {
            current.clear();
            return;
        }
        const std::string decoded = convertBufferWithCodePage(current, codePage);
        if (!decoded.empty()) decodedChunks.push_back(decoded);
        current.clear();
    };

    for (unsigned char byte : data)
    {
        if (byte == '\r' || byte == '\n')
        {
            current.push_back('\n');
        }
        else if (byte == '\t')
        {
            current.push_back('\t');
        }
        else if (byte >= 0x20 || byte >= 0xA1)
        {
            current.push_back(static_cast<char>(byte));
        }
        else
        {
            flushChunk();
        }
    }
    flushChunk();
    return combineCandidateChunks(decodedChunks);
}

static uint16_t codePageForName(const std::string &name)
{
    const std::string lower = toLower(name);
    if (lower == "gb18030" || lower == "gbk" || lower == "gb2312") return 936;
    if (lower == "big5") return 950;
    if (lower == "shift-jis" || lower == "shift_jis" || lower == "sjis") return 932;
    if (lower == "windows-1252" || lower == "cp1252") return 1252;
    return 0;
}

static std::string decodeWithCodecNames(const std::string &data, std::initializer_list<const char *> names)
{
    for (const char *name : names)
    {
        const uint16_t codePage = codePageForName(name);
        if (codePage == 0) continue;
        const std::string decoded = decodeWithCodePage(data, codePage);
        if (!decoded.empty()) return decoded;
    }
    return {};
}

static std::string readWpsViaWordBinary(const std::string &path)
{
    CompoundFileReader reader;
    if (!reader.load(path)) return {};
    const std::string wordStream = reader.streamByName("WordDocument");
    if (wordStream.empty()) return {};
    FibInfo fib;
    if (!parseFib(wordStream, fib)) return {};
    const std::string tableStream = reader.streamByName(fib.useTable1 ? "1Table" : "0Table");
    std::string raw;
    if (!tableStream.empty() && fib.fcClx && fib.lcbClx)
    {
        const std::vector<TextPiece> pieces = parseTextPieces(tableStream, fib.fcClx, fib.lcbClx);
        raw = decodePieces(wordStream, pieces);
    }
    if (raw.empty()) raw = decodeSimpleRange(wordStream, fib.fcMin, fib.fcMac);
    return normalizeWordText(raw);
}

static std::string readWpsHeuristic(const std::string &path)
{
    return extractUtf16Text(readBinaryFile(path));
}

std::string readWpsText(const std::string &path)
{
    std::string parsed = readWpsViaWordBinary(path);
    if (!parsed.empty()) return parsed;
    return readWpsHeuristic(path);
}

static std::string formatMarkdownList(const std::string &text)
{
    const std::vector<std::string> lines = splitLines(text);
    std::vector<std::string> formatted;
    formatted.reserve(lines.size());
    for (const std::string &line : lines)
    {
        const std::string trimmedLine = trim(line);
        if (!trimmedLine.empty()) formatted.push_back("- " + trimmedLine);
    }
    return join(formatted, "\n");
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

static std::string xlsCellToString(const xls::xlsCell *cell)
{
    if (!cell) return {};
    if (cell->str) return std::string(cell->str);

    switch (cell->id)
    {
    case XLS_RECORD_BOOLERR:
        return cell->d != 0.0 ? "TRUE" : "FALSE";
    case XLS_RECORD_NUMBER:
    case XLS_RECORD_RK:
    case XLS_RECORD_FORMULA:
    case XLS_RECORD_FORMULA_ALT:
        if (std::isfinite(cell->d))
        {
            std::ostringstream oss;
            oss << std::setprecision(15) << cell->d;
            return oss.str();
        }
        break;
    default:
        break;
    }
    if (cell->l != 0) return std::to_string(cell->l);
    return {};
}

static std::string readEtViaLibxls(const std::string &path)
{
    xls::xls_error_t err = xls::LIBXLS_OK;
    std::unique_ptr<xls::xlsWorkBook, XlsWorkbookCloser> wb(xls::xls_open_file(path.c_str(), "UTF-8", &err));
    if (!wb) return {};

    std::vector<std::string> sections;
    int sheetIndex = 1;
    for (xls::DWORD i = 0; i < wb->sheets.count; ++i)
    {
        std::unique_ptr<xls::xlsWorkSheet, XlsWorksheetCloser> ws(xls::xls_getWorkSheet(wb.get(), static_cast<int>(i)));
        if (!ws) continue;
        if (xls::xls_parseWorkSheet(ws.get()) != xls::LIBXLS_OK) continue;

        const int lastRow = ws->rows.lastrow;
        const int lastCol = ws->rows.lastcol;
        if (lastRow < 0 || lastCol < 0) continue;

        std::vector<std::vector<std::string>> rows;
        rows.reserve(static_cast<size_t>(lastRow) + 1);
        size_t maxColumns = 0;
        for (int rowIndex = 0; rowIndex <= lastRow; ++rowIndex)
        {
            xls::xlsRow *row = xls::xls_row(ws.get(), static_cast<xls::WORD>(rowIndex));
            if (!row) continue;

            std::vector<std::string> rowCells;
            rowCells.reserve(static_cast<size_t>(lastCol) + 1);
            bool hasContent = false;
            for (int colIndex = 0; colIndex <= lastCol; ++colIndex)
            {
                xls::xlsCell *cell = xls::xls_cell(ws.get(), static_cast<xls::WORD>(rowIndex), static_cast<xls::WORD>(colIndex));
                std::string value = xlsCellToString(cell);
                if (!trim(value).empty()) hasContent = true;
                rowCells.push_back(std::move(value));
            }
            while (!rowCells.empty() && trim(rowCells.back()).empty()) rowCells.pop_back();
            if (!hasContent || rowCells.empty()) continue;
            maxColumns = std::max(maxColumns, rowCells.size());
            rows.push_back(std::move(rowCells));
        }

        if (rows.empty() || maxColumns == 0) continue;
        for (auto &row : rows)
        {
            if (row.size() < maxColumns) row.resize(maxColumns);
        }
        const std::string table = makeMarkdownTable(rows);
        if (table.empty()) continue;
        sections.push_back("## Sheet " + std::to_string(sheetIndex++) + "\n\n" + table);
    }
    return join(sections, "\n\n");
}

std::string readEtText(const std::string &path)
{
    const std::string viaLibxls = readEtViaLibxls(path);
    if (!viaLibxls.empty()) return viaLibxls;

    std::string text;
    CompoundFileReader reader;
    if (reader.load(path))
    {
        const std::string workbook = reader.streamByName("Workbook");
        if (!workbook.empty())
        {
            const uint16_t codePage = detectBiffCodePage(workbook);
            if (codePage != 0 && codePage != 1200)
                text = decodeWithCodePage(workbook, codePage);
            if (text.empty()) text = extractUtf16Text(workbook);
        }
    }
    if (text.empty()) text = readWpsHeuristic(path);
    if (text.empty()) return {};
    return "## ET Workbook\n\n" + text;
}

static bool isPptTextRecordType(uint16_t type)
{
    switch (type)
    {
    case 0x0FA0: // TextCharsAtom
    case 0x0FA8: // TextBytesAtom
    case 0x0FBA: // CString
    case 0x0D45: // SlideNameAtom
        return true;
    default:
        break;
    }
    return false;
}

static bool looksUtf16TextPayload(const std::string &payload)
{
    if (payload.size() < 4 || (payload.size() & 1)) return false;
    const size_t charCount = payload.size() / 2;
    if (charCount == 0) return false;
    size_t printable = 0;
    for (size_t i = 0; i + 1 < payload.size(); i += 2)
    {
        const uint16_t value = readLe<uint16_t>(reinterpret_cast<const uint8_t *>(payload.data() + i));
        if (value == 0x0000) continue;
        if (value == 0x0009 || value == 0x000A || value == 0x000D)
        {
            ++printable;
            continue;
        }
        if (isLikelyPrintableWordChar(value)) ++printable;
    }
    return printable * 2 >= charCount;
}

static bool looksLatinTextPayload(const std::string &payload)
{
    if (payload.size() < 3) return false;
    size_t printable = 0;
    for (unsigned char ch : payload)
    {
        if ((ch >= 32 && ch <= 126) || ch == '\r' || ch == '\n' || ch == '\t')
            ++printable;
    }
    return printable * 2 >= payload.size();
}

static std::string decodePptBytes(const std::string &payload)
{
    if (isValidUtf8(payload)) return payload;
    static const uint32_t codePages[] = {936, 950, 932, 1252};
    for (uint32_t cp : codePages)
    {
        const std::string decoded = convertBufferWithCodePage(payload, cp);
        if (!decoded.empty()) return decoded;
    }
    return latin1ToUtf8(payload);
}

static std::string decodePptTextRecord(uint16_t type, const std::string &payload)
{
    if (!isPptTextRecordType(type) || payload.empty()) return {};
    std::string decoded;
    if (looksUtf16TextPayload(payload))
    {
        const char16_t *chars = reinterpret_cast<const char16_t *>(payload.data());
        decoded = utf16ToUtf8(chars, payload.size() / 2);
    }
    else if (looksLatinTextPayload(payload))
    {
        decoded = decodePptBytes(payload);
    }
    else
    {
        return {};
    }

    std::string cleaned;
    cleaned.reserve(decoded.size());
    for (unsigned char byte : decoded)
    {
        if (byte == 0x00) continue;
        if (byte == 0x0D || byte == 0x0B)
            cleaned.push_back('\n');
        else if (byte == 0x09)
            cleaned.push_back(' ');
        else if (byte >= 0x20 || static_cast<int8_t>(byte) < 0)
            cleaned.push_back(static_cast<char>(byte));
    }
    return cleaned;
}

struct PptTextBucket
{
    std::vector<std::string> lines;
    std::unordered_set<std::string> seen;
};

static void collectPptTextRecords(const std::string &data,
                                  int offset,
                                  int length,
                                  std::vector<PptTextBucket> &slides,
                                  PptTextBucket &loose,
                                  int currentSlideIndex = -1)
{
    const int end = offset + length;
    int pos = offset;
    while (pos + 8 <= end)
    {
        const uint8_t *ptr = reinterpret_cast<const uint8_t *>(data.data() + pos);
        const uint16_t verInst = readLe<uint16_t>(ptr);
        const uint16_t type = readLe<uint16_t>(ptr + 2);
        const uint32_t size = readLe<uint32_t>(ptr + 4);
        const int bodyStart = pos + 8;
        const int bodyEnd = bodyStart + static_cast<int>(size);
        if (bodyEnd > end || bodyEnd > static_cast<int>(data.size())) break;

        const uint16_t recVer = static_cast<uint16_t>(verInst & 0x000F);
        const bool treatAsSlideContainer = (size > 0) &&
                                           ((recVer == 0x000F && (type == 0x03EE || type == 0x03F8 || type == 0x0FF0)) ||
                                            type == 0x0FF1);
        if (treatAsSlideContainer)
        {
                slides.emplace_back();
                const int newIndex = static_cast<int>(slides.size()) - 1;
                collectPptTextRecords(data, bodyStart, static_cast<int>(size), slides, loose, newIndex);
                if (slides.back().lines.empty()) slides.pop_back();
        }
        else if (recVer == 0x000F && size > 0)
        {
            collectPptTextRecords(data, bodyStart, static_cast<int>(size), slides, loose, currentSlideIndex);
        }
        else if (size > 0)
        {
            const std::string payload(data.data() + bodyStart, static_cast<size_t>(size));
            const std::string decoded = decodePptTextRecord(type, payload);
            if (!decoded.empty())
            {
                const std::vector<std::string> lines = splitLines(decoded);
                for (const std::string &line : lines)
                {
                    const std::string trimmedLine = trim(line);
                    if (trimmedLine.empty()) continue;
                    if (trimmedLine.find("\xEF\xBF\xBD") != std::string::npos) continue;
                    if (!looksLikeDocumentText(trimmedLine)) continue;
                    PptTextBucket *target = (currentSlideIndex >= 0) ? &slides[static_cast<size_t>(currentSlideIndex)] : &loose;
                    if (target->seen.insert(trimmedLine).second) target->lines.push_back(trimmedLine);
                }
            }
        }
        pos = bodyEnd;
    }
}

static std::string formatDpsSlides(const std::vector<PptTextBucket> &slides)
{
    if (slides.empty()) return {};
    std::vector<std::string> sections;
    sections.reserve(slides.size());
    int slideIndex = 1;
    for (const auto &slide : slides)
    {
        if (slide.lines.empty()) continue;
        const std::string list = formatMarkdownList(join(slide.lines, "\n"));
        if (list.empty()) continue;
        sections.push_back("## Slide " + std::to_string(slideIndex++) + "\n\n" + list);
    }
    if (sections.empty()) return {};
    return join(sections, "\n\n");
}

static std::string readDpsViaPptBinary(const std::string &path)
{
    CompoundFileReader reader;
    if (!reader.load(path)) return {};
    const std::string stream = reader.streamByName("PowerPoint Document");
    if (stream.empty()) return {};

    std::vector<PptTextBucket> slides;
    PptTextBucket loose;
    collectPptTextRecords(stream, 0, static_cast<int>(stream.size()), slides, loose, -1);
    const std::string structured = formatDpsSlides(slides);
    if (!structured.empty()) return structured;
    if (loose.lines.empty()) return {};
    return join(loose.lines, "\n");
}

std::string readDpsText(const std::string &path)
{
    std::string text = readDpsViaPptBinary(path);
    if (text.empty())
    {
        CompoundFileReader reader;
        if (reader.load(path))
        {
            const std::string stream = reader.streamByName("PowerPoint Document");
            if (!stream.empty())
            {
                text = extractUtf16Text(stream);
                if (text.empty())
                    text = decodeWithCodecNames(stream, {"GB18030", "Big5", "Shift-JIS", "Windows-1252"});
            }
        }
        if (text.empty()) text = readWpsHeuristic(path);
    }
    if (text.empty()) return {};
    if (text.rfind("## Slide ", 0) == 0) return text;
    const std::string list = formatMarkdownList(text);
    if (list.empty()) return text;
    return "## DPS Slides\n\n" + list;
}

static const tinyxml2::XMLElement *findChildElement(const tinyxml2::XMLElement *root, const char *name)
{
    for (auto element = root ? root->FirstChildElement() : nullptr; element; element = element->NextSiblingElement())
    {
        if (std::strcmp(element->Name(), name) == 0) return element;
    }
    return nullptr;
}

static void collectWordText(const tinyxml2::XMLElement *node, std::string &out)
{
    if (!node) return;
    const char *name = node->Name();
    if (std::strcmp(name, "w:t") == 0)
    {
        if (const char *text = node->GetText()) out += text;
        return;
    }
    if (std::strcmp(name, "w:br") == 0 || std::strcmp(name, "w:cr") == 0)
    {
        out.push_back('\n');
        return;
    }
    if (std::strcmp(name, "w:tab") == 0)
    {
        out.push_back(' ');
        return;
    }
    for (auto child = node->FirstChildElement(); child; child = child->NextSiblingElement())
        collectWordText(child, out);
}

static std::string docxParagraphStyle(const tinyxml2::XMLElement *paragraph)
{
    const auto *pPr = findChildElement(paragraph, "w:pPr");
    if (!pPr) return {};
    const auto *style = findChildElement(pPr, "w:pStyle");
    if (!style) return {};
    if (const char *val = style->Attribute("w:val")) return val;
    if (const char *legacy = style->Attribute("val")) return legacy;
    return {};
}

static std::string formatDocxParagraph(const tinyxml2::XMLElement *paragraph)
{
    std::string text;
    collectWordText(paragraph, text);
    const std::string trimmedText = trim(text);
    if (trimmedText.empty()) return {};
    const std::string style = docxParagraphStyle(paragraph);
    if (!style.empty() && style.size() > 7 && style.substr(0, 7) == "Heading")
    {
        try
        {
            int level = std::stoi(style.substr(7));
            level = std::max(1, std::min(6, level));
            return std::string(static_cast<size_t>(level), '#') + " " + trimmedText;
        }
        catch (...)
        {
            // fall back to plain text
        }
    }
    return trimmedText;
}

static std::string parseDocxTable(const tinyxml2::XMLElement *tbl);

static std::string readDocxTableCell(const tinyxml2::XMLElement *cell)
{
    std::vector<std::string> fragments;
    for (auto child = cell->FirstChildElement(); child; child = child->NextSiblingElement())
    {
        const char *name = child->Name();
        if (std::strcmp(name, "w:p") == 0)
        {
            const std::string paragraph = formatDocxParagraph(child);
            if (!paragraph.empty()) fragments.push_back(paragraph);
        }
        else if (std::strcmp(name, "w:tbl") == 0)
        {
            const std::string nested = parseDocxTable(child);
            if (!nested.empty()) fragments.push_back(nested);
        }
    }
    return join(fragments, "\n");
}

static std::string parseDocxTable(const tinyxml2::XMLElement *tbl)
{
    std::vector<std::vector<std::string>> rows;
    for (auto row = tbl->FirstChildElement("w:tr"); row; row = row->NextSiblingElement("w:tr"))
    {
        std::vector<std::string> cells;
        for (auto cell = row->FirstChildElement("w:tc"); cell; cell = cell->NextSiblingElement("w:tc"))
            cells.push_back(readDocxTableCell(cell));
        if (!cells.empty()) rows.push_back(cells);
    }
    return makeMarkdownTable(rows);
}

static std::string parseDocxDocumentXml(const std::string &xml)
{
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) return {};
    const auto *root = doc.RootElement();
    if (!root) return {};
    const auto *body = findChildElement(root, "w:body");
    if (!body) return {};
    std::vector<std::string> blocks;
    for (auto node = body->FirstChildElement(); node; node = node->NextSiblingElement())
    {
        const char *name = node->Name();
        if (std::strcmp(name, "w:p") == 0)
        {
            const std::string block = formatDocxParagraph(node);
            if (!block.empty()) blocks.push_back(block);
        }
        else if (std::strcmp(name, "w:tbl") == 0)
        {
            const std::string table = parseDocxTable(node);
            if (!table.empty()) blocks.push_back(table);
        }
    }
    return join(blocks, "\n\n");
}

std::string readDocxText(const std::string &path)
{
    ZipArchive zip;
    if (!zip.open(path)) return {};
    const std::string xml = zip.fileContent("word/document.xml");
    if (xml.empty()) return {};
    return parseDocxDocumentXml(xml);
}

static void skipPdfWhitespace(const std::string &data, size_t &pos)
{
    while (pos < data.size())
    {
        const unsigned char ch = static_cast<unsigned char>(data[pos]);
        if (ch == '%')
        {
            while (pos < data.size() && data[pos] != '\n' && data[pos] != '\r') ++pos;
        }
        else if (std::isspace(ch))
        {
            ++pos;
        }
        else
        {
            break;
        }
    }
}

static int pdfHexDigit(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
    if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
    return -1;
}

static std::string parsePdfLiteralString(const std::string &data, size_t &pos)
{
    std::string out;
    if (pos >= data.size() || data[pos] != '(') return out;
    ++pos;
    int depth = 1;
    while (pos < data.size() && depth > 0)
    {
        char ch = data[pos++];
        if (ch == '\\')
        {
            if (pos >= data.size()) break;
            char next = data[pos++];
            switch (next)
            {
            case 'n':
                out.push_back('\n');
                break;
            case 'r':
                out.push_back('\r');
                break;
            case 't':
                out.push_back('\t');
                break;
            case 'b':
                out.push_back('\b');
                break;
            case 'f':
                out.push_back('\f');
                break;
            case '(':
            case ')':
            case '\\':
                out.push_back(next);
                break;
            case '\r':
                if (pos < data.size() && data[pos] == '\n') ++pos;
                break;
            case '\n':
                break;
            default:
                if (next >= '0' && next <= '7')
                {
                    int value = next - '0';
                    for (int i = 0; i < 2 && pos < data.size(); ++i)
                    {
                        char digit = data[pos];
                        if (digit < '0' || digit > '7') break;
                        value = value * 8 + (digit - '0');
                        ++pos;
                    }
                    out.push_back(static_cast<char>(value & 0xFF));
                }
                else
                {
                    out.push_back(next);
                }
                break;
            }
        }
        else if (ch == '(')
        {
            ++depth;
            out.push_back(ch);
        }
        else if (ch == ')')
        {
            --depth;
            if (depth > 0) out.push_back(')');
        }
        else
        {
            out.push_back(ch);
        }
    }
    return out;
}

static std::string parsePdfHexString(const std::string &data, size_t &pos)
{
    std::string out;
    if (pos >= data.size() || data[pos] != '<' || (pos + 1 < data.size() && data[pos + 1] == '<')) return out;
    ++pos;
    std::string buffer;
    while (pos < data.size())
    {
        char ch = data[pos++];
        if (ch == '>') break;
        if (std::isspace(static_cast<unsigned char>(ch))) continue;
        buffer.push_back(ch);
    }
    if (buffer.empty()) return {};
    if (buffer.size() % 2 != 0) buffer.push_back('0');
    for (size_t i = 0; i + 1 < buffer.size(); i += 2)
    {
        const int hi = pdfHexDigit(buffer[i]);
        const int lo = pdfHexDigit(buffer[i + 1]);
        if (hi < 0 || lo < 0) continue;
        out.push_back(static_cast<char>((hi << 4) | lo));
    }
    return out;
}

static std::vector<std::string> parsePdfArrayStrings(const std::string &data, size_t &pos)
{
    std::vector<std::string> values;
    if (pos >= data.size() || data[pos] != '[') return values;
    ++pos;
    while (pos < data.size())
    {
        skipPdfWhitespace(data, pos);
        if (pos >= data.size()) break;
        char ch = data[pos];
        if (ch == ']')
        {
            ++pos;
            break;
        }
        if (ch == '(')
        {
            values.push_back(parsePdfLiteralString(data, pos));
        }
        else if (ch == '<' && (pos + 1 >= data.size() || data[pos + 1] != '<'))
        {
            values.push_back(parsePdfHexString(data, pos));
        }
        else
        {
            ++pos;
        }
    }
    return values;
}

struct PdfFontMap
{
    std::unordered_map<std::string, std::string> glyphMap;
    size_t maxGlyphLength = 1;
};

static std::string hexStringToBytes(std::string hex)
{
    if (hex.empty()) return {};
    if (hex.size() % 2 != 0) hex.insert(hex.begin(), '0');
    std::string bytes;
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2)
    {
        const int hi = pdfHexDigit(hex[i]);
        const int lo = pdfHexDigit(hex[i + 1]);
        if (hi < 0 || lo < 0) return {};
        bytes.push_back(static_cast<char>((hi << 4) | lo));
    }
    return bytes;
}

static uint32_t hexStringToUInt(const std::string &hex)
{
    uint32_t value = 0;
    for (char ch : hex)
    {
        const int digit = pdfHexDigit(ch);
        if (digit < 0) return 0;
        value = (value << 4) | static_cast<uint32_t>(digit);
    }
    return value;
}

static std::string unicodeFromHex(const std::string &hex)
{
    if (hex.empty()) return {};
    const std::string bytes = hexStringToBytes(hex);
    if (bytes.empty() || (bytes.size() % 2) != 0) return {};
    std::u16string u16;
    for (size_t i = 0; i + 1 < bytes.size(); i += 2)
    {
        const char16_t value = static_cast<char16_t>((static_cast<unsigned char>(bytes[i]) << 8) |
                                                     static_cast<unsigned char>(bytes[i + 1]));
        u16.push_back(value);
    }
    return utf16ToUtf8(u16);
}

static std::string encodeCodeToBytes(uint32_t value, size_t byteLength)
{
    std::string bytes(byteLength, '\0');
    for (size_t i = 0; i < byteLength; ++i)
    {
        bytes[byteLength - 1 - i] = static_cast<char>(value & 0xFFu);
        value >>= 8;
    }
    return bytes;
}

static std::string codePointToUtf8(uint32_t codePoint)
{
    std::string out;
    appendUtf8CodePoint(out, codePoint);
    return out;
}

static bool extractNextHexToken(const std::string &line, size_t &pos, std::string &token)
{
    const size_t start = line.find('<', pos);
    if (start == std::string::npos) return false;
    const size_t end = line.find('>', start);
    if (end == std::string::npos) return false;
    token = line.substr(start + 1, end - start - 1);
    pos = end + 1;
    return true;
}

static PdfFontMap parseToUnicodeCMap(const std::string &cmap)
{
    PdfFontMap map;
    std::istringstream stream(cmap);
    std::string line;
    int mode = 0;
    int remaining = 0;
    while (std::getline(stream, line))
    {
        const std::string trimmedLine = trim(line);
        if (trimmedLine.empty()) continue;
        if (trimmedLine.find("beginbfchar") != std::string::npos)
        {
            remaining = 0;
            std::istringstream ss(trimmedLine);
            ss >> remaining;
            mode = 1;
            continue;
        }
        if (trimmedLine.find("beginbfrange") != std::string::npos)
        {
            remaining = 0;
            std::istringstream ss(trimmedLine);
            ss >> remaining;
            mode = 2;
            continue;
        }
        if (trimmedLine.find("endbfchar") != std::string::npos || trimmedLine.find("endbfrange") != std::string::npos)
        {
            mode = 0;
            continue;
        }
        if (mode == 1 && remaining > 0)
        {
            size_t pos = 0;
            std::string srcHex;
            std::string dstHex;
            if (!extractNextHexToken(trimmedLine, pos, srcHex)) continue;
            if (!extractNextHexToken(trimmedLine, pos, dstHex)) continue;
            const std::string key = hexStringToBytes(srcHex);
            const std::string value = unicodeFromHex(dstHex);
            if (!key.empty() && !value.empty())
            {
                map.maxGlyphLength = std::max(map.maxGlyphLength, key.size());
                map.glyphMap[key] = value;
            }
            --remaining;
            continue;
        }
        if (mode == 2 && remaining > 0)
        {
            size_t pos = 0;
            std::string startHex;
            std::string endHex;
            if (!extractNextHexToken(trimmedLine, pos, startHex)) continue;
            if (!extractNextHexToken(trimmedLine, pos, endHex)) continue;
            const size_t codeBytes = startHex.size() / 2;
            const uint32_t startCode = hexStringToUInt(startHex);
            const uint32_t endCode = hexStringToUInt(endHex);
            if (trimmedLine.find('[', pos) != std::string::npos)
            {
                size_t arrayPos = trimmedLine.find('[', pos);
                ++arrayPos;
                std::vector<std::string> destinations;
                while (arrayPos < trimmedLine.size() && trimmedLine[arrayPos] != ']')
                {
                    std::string destHex;
                    if (extractNextHexToken(trimmedLine, arrayPos, destHex))
                        destinations.push_back(destHex);
                    else
                        ++arrayPos;
                }
                uint32_t glyphCode = startCode;
                for (size_t i = 0; i < destinations.size() && glyphCode <= endCode; ++i, ++glyphCode)
                {
                    const std::string key = encodeCodeToBytes(glyphCode, codeBytes);
                    const std::string value = unicodeFromHex(destinations[i]);
                    if (!key.empty() && !value.empty())
                    {
                        map.maxGlyphLength = std::max(map.maxGlyphLength, key.size());
                        map.glyphMap[key] = value;
                    }
                }
            }
            else
            {
                std::string destHex;
                if (!extractNextHexToken(trimmedLine, pos, destHex)) continue;
                uint32_t destValue = hexStringToUInt(destHex);
                for (uint32_t glyph = startCode; glyph <= endCode; ++glyph, ++destValue)
                {
                    const std::string key = encodeCodeToBytes(glyph, codeBytes);
                    const std::string value = codePointToUtf8(destValue);
                    if (!key.empty())
                    {
                        map.maxGlyphLength = std::max(map.maxGlyphLength, key.size());
                        map.glyphMap[key] = value;
                    }
                }
            }
            --remaining;
        }
    }
    return map;
}

static std::unordered_map<std::string, int> extractFontResourceTargets(const std::string &pdf)
{
    std::unordered_map<std::string, int> fonts;
    size_t pos = 0;
    while ((pos = pdf.find('/', pos)) != std::string::npos)
    {
        size_t nameStart = pos + 1;
        size_t nameEnd = nameStart;
        while (nameEnd < pdf.size() &&
               (std::isalnum(static_cast<unsigned char>(pdf[nameEnd])) || pdf[nameEnd] == '_' || pdf[nameEnd] == '-'))
            ++nameEnd;
        if (nameEnd == nameStart)
        {
            pos = nameEnd;
            continue;
        }
        const std::string name = pdf.substr(nameStart, nameEnd - nameStart);
        size_t cursor = nameEnd;
        while (cursor < pdf.size() && std::isspace(static_cast<unsigned char>(pdf[cursor]))) ++cursor;
        const size_t numStart = cursor;
        while (cursor < pdf.size() && std::isdigit(static_cast<unsigned char>(pdf[cursor]))) ++cursor;
        if (numStart == cursor)
        {
            pos = cursor;
            continue;
        }
        int objectNumber = -1;
        try
        {
            objectNumber = std::stoi(pdf.substr(numStart, cursor - numStart));
        }
        catch (...)
        {
            pos = cursor;
            continue;
        }
        while (cursor < pdf.size() && std::isspace(static_cast<unsigned char>(pdf[cursor]))) ++cursor;
        if (cursor >= pdf.size() || pdf[cursor] != '0')
        {
            pos = cursor;
            continue;
        }
        ++cursor;
        while (cursor < pdf.size() && std::isspace(static_cast<unsigned char>(pdf[cursor]))) ++cursor;
        if (cursor >= pdf.size() || pdf[cursor] != 'R')
        {
            pos = cursor;
            continue;
        }
        fonts[name] = objectNumber;
        pos = cursor + 1;
    }
    return fonts;
}

static std::string pdfObjectContent(const std::string &data, int objectNumber)
{
    if (objectNumber < 0) return {};
    const std::string marker = std::to_string(objectNumber) + " 0 obj";
    size_t pos = data.find(marker);
    if (pos == std::string::npos) return {};
    pos += marker.size();
    const size_t end = data.find("endobj", pos);
    if (end == std::string::npos) return {};
    return data.substr(pos, end - pos);
}

static int extractToUnicodeObject(const std::string &pdf, int fontObjectNumber)
{
    const std::string body = pdfObjectContent(pdf, fontObjectNumber);
    if (body.empty()) return -1;
    const size_t marker = body.find("/ToUnicode");
    if (marker == std::string::npos) return -1;
    size_t pos = marker + 10;
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) ++pos;
    const size_t numStart = pos;
    while (pos < body.size() && std::isdigit(static_cast<unsigned char>(body[pos]))) ++pos;
    if (numStart == pos) return -1;
    try
    {
        return std::stoi(body.substr(numStart, pos - numStart));
    }
    catch (...)
    {
        return -1;
    }
}

static std::unordered_map<std::string, PdfFontMap>
buildPdfFontMaps(const std::string &pdf,
                 const std::unordered_map<int, std::string> &streamByObject,
                 std::unordered_set<int> &toUnicodeObjects)
{
    std::unordered_map<std::string, PdfFontMap> fontMaps;
    const std::unordered_map<std::string, int> fontTargets = extractFontResourceTargets(pdf);
    for (const auto &entry : fontTargets)
    {
        const int fontObject = entry.second;
        const int toUnicodeObject = extractToUnicodeObject(pdf, fontObject);
        if (toUnicodeObject < 0) continue;
        const auto streamIt = streamByObject.find(toUnicodeObject);
        if (streamIt == streamByObject.end()) continue;
        fontMaps[entry.first] = parseToUnicodeCMap(streamIt->second);
        toUnicodeObjects.insert(toUnicodeObject);
    }
    return fontMaps;
}

static std::string decodePdfGlyphs(const PdfFontMap *font, const std::string &raw)
{
    if (!font || font->glyphMap.empty()) return raw;
    std::string out;
    size_t index = 0;
    while (index < raw.size())
    {
        const size_t remaining = raw.size() - index;
        size_t maxLen = std::min(remaining, font->maxGlyphLength);
        bool matched = false;
        while (maxLen > 0)
        {
            const std::string key = raw.substr(index, maxLen);
            const auto it = font->glyphMap.find(key);
            if (it != font->glyphMap.end())
            {
                out += it->second;
                index += maxLen;
                matched = true;
                break;
            }
            --maxLen;
        }
        if (!matched)
        {
            out.push_back(raw[index]);
            ++index;
        }
    }
    return out;
}

static int pdfObjectNumberBefore(const std::string &data, size_t streamPos)
{
    size_t searchPos = streamPos;
    while (true)
    {
        const size_t objPos = data.rfind("obj", searchPos);
        if (objPos == std::string::npos) return -1;
        if (objPos == 0) return -1;
        size_t cursor = objPos;
        while (cursor > 0 && std::isspace(static_cast<unsigned char>(data[cursor - 1]))) --cursor;
        size_t genEnd = cursor;
        size_t genStart = genEnd;
        while (genStart > 0 && std::isdigit(static_cast<unsigned char>(data[genStart - 1]))) --genStart;
        if (genStart == genEnd)
        {
            searchPos = objPos - 1;
            continue;
        }
        cursor = genStart;
        while (cursor > 0 && std::isspace(static_cast<unsigned char>(data[cursor - 1]))) --cursor;
        size_t idEnd = cursor;
        size_t idStart = idEnd;
        while (idStart > 0 && std::isdigit(static_cast<unsigned char>(data[idStart - 1]))) --idStart;
        if (idStart == idEnd)
        {
            searchPos = objPos - 1;
            continue;
        }
        try
        {
            return std::stoi(data.substr(idStart, idEnd - idStart));
        }
        catch (...)
        {
            searchPos = objPos - 1;
            continue;
        }
    }
}

static void appendPdfLine(std::vector<std::string> &lines, std::string &current)
{
    const std::string trimmed = trim(current);
    if (!trimmed.empty()) lines.push_back(trimmed);
    current.clear();
}

static std::string parsePdfTextStream(const std::string &data, const std::unordered_map<std::string, PdfFontMap> &fonts)
{
    bool inText = false;
    std::string current;
    std::vector<std::string> lines;
    size_t pos = 0;
    std::string pendingFontName;
    const PdfFontMap *activeFont = nullptr;
    auto appendText = [&](const std::string &text) {
        const std::string decoded = decodePdfGlyphs(activeFont, text);
        if (decoded.empty()) return;
        current += decoded;
    };

    while (pos < data.size())
    {
        skipPdfWhitespace(data, pos);
        if (pos >= data.size()) break;
        char ch = data[pos];
        if (ch == '%')
        {
            while (pos < data.size() && data[pos] != '\n' && data[pos] != '\r') ++pos;
            continue;
        }
        if (ch == '(' && inText)
        {
            appendText(parsePdfLiteralString(data, pos));
            continue;
        }
        if (ch == '[' && inText)
        {
            const std::vector<std::string> arrayStrings = parsePdfArrayStrings(data, pos);
            for (const std::string &value : arrayStrings) appendText(value);
            continue;
        }
        if (ch == '<' && inText && (pos + 1 >= data.size() || data[pos + 1] != '<'))
        {
            appendText(parsePdfHexString(data, pos));
            continue;
        }
        if (ch == '\'' && inText)
        {
            ++pos;
            appendPdfLine(lines, current);
            continue;
        }
        if (ch == '"' && inText)
        {
            ++pos;
            appendPdfLine(lines, current);
            continue;
        }
        if (ch == 'T' && inText && pos + 1 < data.size() && data[pos + 1] == '*')
        {
            pos += 2;
            appendPdfLine(lines, current);
            continue;
        }
        if (ch == '/' && inText)
        {
            size_t nameStart = pos + 1;
            size_t nameEnd = nameStart;
            while (nameEnd < data.size() &&
                   (std::isalnum(static_cast<unsigned char>(data[nameEnd])) || data[nameEnd] == '_' || data[nameEnd] == '-'))
                ++nameEnd;
            pendingFontName = data.substr(nameStart, nameEnd - nameStart);
            pos = nameEnd;
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(ch)))
        {
            const size_t start = pos;
            while (pos < data.size() && std::isalpha(static_cast<unsigned char>(data[pos]))) ++pos;
            const std::string token = data.substr(start, pos - start);
            if (token == "BT")
            {
                current.clear();
                inText = true;
            }
            else if (token == "ET")
            {
                appendPdfLine(lines, current);
                inText = false;
            }
            else if (token == "Tf")
            {
                const auto fontIt = fonts.find(pendingFontName);
                activeFont = (fontIt != fonts.end()) ? &fontIt->second : nullptr;
            }
            continue;
        }
        if ((ch == '+' || ch == '-' || ch == '.' || (ch >= '0' && ch <= '9')))
        {
            while (pos < data.size() &&
                   (std::isdigit(static_cast<unsigned char>(data[pos])) || data[pos] == '+' || data[pos] == '-' ||
                    data[pos] == '.' || data[pos] == 'E' || data[pos] == 'e'))
                ++pos;
            continue;
        }
        ++pos;
    }
    appendPdfLine(lines, current);
    return join(lines, "\n\n");
}

static std::string decompressPdfStreamIfNeeded(const std::string &dict, const std::string &data)
{
    if (dict.find("/Filter") == std::string::npos || dict.find("/FlateDecode") == std::string::npos)
        return data;
    size_t outSize = 0;
    void *buffer = tinfl_decompress_mem_to_heap(data.data(), data.size(), &outSize, TINFL_FLAG_PARSE_ZLIB_HEADER);
    if (!buffer || outSize == 0)
    {
        if (buffer) mz_free(buffer);
        return {};
    }
    std::string result(static_cast<const char *>(buffer), outSize);
    mz_free(buffer);
    return result;
}

struct PdfStreamInfo
{
    int objectNumber = -1;
    std::string dict;
    std::string decoded;
};

static size_t pdfStreamDeclaredLength(const std::string &dict)
{
    const std::string key = "/Length";
    size_t pos = dict.find(key);
    if (pos == std::string::npos) return 0;
    pos += key.size();
    while (pos < dict.size() && std::isspace(static_cast<unsigned char>(dict[pos]))) ++pos;
    const size_t start = pos;
    while (pos < dict.size() && std::isdigit(static_cast<unsigned char>(dict[pos]))) ++pos;
    if (start == pos) return 0;
    try
    {
        return static_cast<size_t>(std::stoull(dict.substr(start, pos - start)));
    }
    catch (...)
    {
        return 0;
    }
}

std::string readPdfText(const std::string &path)
{
    const std::string fileData = readBinaryFile(path);
    if (fileData.empty()) return {};
    std::vector<PdfStreamInfo> streams;
    std::unordered_map<int, std::string> streamByObject;
    size_t searchPos = 0;
    while (true)
    {
        const size_t streamPos = fileData.find("stream", searchPos);
        if (streamPos == std::string::npos) break;
        size_t dictStart = fileData.rfind("<<", streamPos);
        size_t dictEnd = fileData.find(">>", dictStart);
        if (dictStart == std::string::npos || dictEnd == std::string::npos || dictEnd > streamPos)
        {
            searchPos = streamPos + 6;
            continue;
        }
        const std::string dict = fileData.substr(dictStart, dictEnd - dictStart + 2);
        if (dict.find("/Subtype /Image") != std::string::npos)
        {
            searchPos = streamPos + 6;
            continue;
        }
        size_t dataStart = streamPos + 6;
        if (dataStart < fileData.size() && fileData[dataStart] == '\r') ++dataStart;
        if (dataStart < fileData.size() && fileData[dataStart] == '\n') ++dataStart;
        const size_t declaredLength = pdfStreamDeclaredLength(dict);
        size_t dataEnd = dataStart;
        size_t endStreamPos = std::string::npos;
        if (declaredLength > 0 && dataStart + declaredLength <= fileData.size())
        {
            dataEnd = dataStart + declaredLength;
            endStreamPos = fileData.find("endstream", dataEnd);
        }
        else
        {
            endStreamPos = fileData.find("endstream", dataStart);
            dataEnd = endStreamPos;
        }
        if (dataEnd == std::string::npos || endStreamPos == std::string::npos) break;
        const std::string rawStream = fileData.substr(dataStart, dataEnd - dataStart);
        const std::string decoded = decompressPdfStreamIfNeeded(dict, rawStream);
        PdfStreamInfo info;
        info.objectNumber = pdfObjectNumberBefore(fileData, streamPos);
        info.dict = dict;
        info.decoded = decoded.empty() ? rawStream : decoded;
        streams.push_back(info);
        if (info.objectNumber >= 0) streamByObject[info.objectNumber] = info.decoded;
        searchPos = endStreamPos + 9;
    }
    if (streams.empty()) return {};

    std::unordered_set<int> toUnicodeObjects;
    const std::unordered_map<std::string, PdfFontMap> fontMaps =
        buildPdfFontMaps(fileData, streamByObject, toUnicodeObjects);

    std::vector<std::string> blocks;
    for (const auto &info : streams)
    {
        if (info.dict.find("/Subtype /Image") != std::string::npos) continue;
        if (info.objectNumber >= 0 && toUnicodeObjects.count(info.objectNumber)) continue;
        const std::string text = parsePdfTextStream(info.decoded, fontMaps);
        const std::string trimmed = trim(text);
        if (!trimmed.empty()) blocks.push_back(trimmed);
    }
    return join(blocks, "\n\n");
}

static std::string readZipEntry(const std::string &path, const std::string &entry)
{
    ZipArchive archive;
    if (!archive.open(path)) return {};
    return archive.fileContent(entry);
}

static void appendOdfTextNode(const tinyxml2::XMLNode *node, std::string &out)
{
    if (!node) return;
    if (const auto *text = node->ToText())
    {
        out += text->Value();
        return;
    }
    const auto *element = node->ToElement();
    if (!element) return;
    const char *name = element->Name();
    if (!name) return;
    if (std::strcmp(name, "text:line-break") == 0)
    {
        out.push_back('\n');
        return;
    }
    if (std::strcmp(name, "text:tab") == 0)
    {
        out.push_back('\t');
        return;
    }
    if (std::strcmp(name, "text:s") == 0)
    {
        const int count = std::max(1, element->IntAttribute("text:c", 1));
        out.append(static_cast<size_t>(count), ' ');
        return;
    }
    for (auto child = element->FirstChild(); child; child = child->NextSibling())
        appendOdfTextNode(child, out);
}

static std::string odfParagraphText(const tinyxml2::XMLElement *element)
{
    if (!element) return {};
    std::string text;
    for (auto node = element->FirstChild(); node; node = node->NextSibling())
        appendOdfTextNode(node, text);
    return trim(text);
}

static std::vector<std::vector<std::string>> parseOdfTableElement(const tinyxml2::XMLElement *tableElement)
{
    std::vector<std::vector<std::string>> rows;
    if (!tableElement) return rows;
    for (auto row = tableElement->FirstChildElement("table:table-row"); row; row = row->NextSiblingElement("table:table-row"))
    {
        std::vector<std::string> cells;
        for (auto cell = row->FirstChildElement(); cell; cell = cell->NextSiblingElement())
        {
            const char *cellName = cell->Name();
            if (!cellName) continue;
            int repeat = cell->IntAttribute("table:number-columns-repeated", 1);
            if (repeat < 1) repeat = 1;
            std::string value;
            if (std::strcmp(cellName, "table:table-cell") == 0)
            {
                std::vector<std::string> fragments;
                for (auto p = cell->FirstChildElement("text:p"); p; p = p->NextSiblingElement("text:p"))
                {
                    const std::string text = odfParagraphText(p);
                    if (!text.empty()) fragments.push_back(text);
                }
                value = join(fragments, "\n");
            }
            if (std::strcmp(cellName, "table:table-cell") != 0 && std::strcmp(cellName, "table:covered-table-cell") != 0)
                continue;
            for (int i = 0; i < repeat; ++i) cells.push_back(value);
        }
        int rowRepeat = row->IntAttribute("table:number-rows-repeated", 1);
        if (rowRepeat < 1) rowRepeat = 1;
        if (cells.empty())
            cells.emplace_back();
        for (int i = 0; i < rowRepeat; ++i) rows.push_back(cells);
    }
    return rows;
}

static void collectOdtListItems(const tinyxml2::XMLElement *list, std::vector<std::string> &items, const std::string &prefix = "")
{
    if (!list) return;
    for (auto item = list->FirstChildElement("text:list-item"); item; item = item->NextSiblingElement("text:list-item"))
    {
        for (auto p = item->FirstChildElement("text:p"); p; p = p->NextSiblingElement("text:p"))
        {
            const std::string text = odfParagraphText(p);
            if (!text.empty()) items.push_back(prefix + "- " + text);
        }
        for (auto nested = item->FirstChildElement("text:list"); nested; nested = nested->NextSiblingElement("text:list"))
            collectOdtListItems(nested, items, prefix + "  ");
    }
}

static void collectOdtElements(const tinyxml2::XMLElement *parent, std::vector<std::string> &blocks)
{
    for (auto element = parent ? parent->FirstChildElement() : nullptr; element; element = element->NextSiblingElement())
    {
        const char *name = element->Name();
        if (!name) continue;
        if (std::strcmp(name, "text:p") == 0)
        {
            const std::string text = odfParagraphText(element);
            if (!text.empty()) blocks.push_back(text);
        }
        else if (std::strcmp(name, "text:h") == 0)
        {
            const std::string text = odfParagraphText(element);
            if (text.empty()) continue;
            int level = element->IntAttribute("text:outline-level", 1);
            level = std::max(1, std::min(6, level));
            blocks.push_back(std::string(static_cast<size_t>(level), '#') + " " + text);
        }
        else if (std::strcmp(name, "text:list") == 0)
        {
            std::vector<std::string> listItems;
            collectOdtListItems(element, listItems);
            if (!listItems.empty()) blocks.push_back(join(listItems, "\n"));
        }
        else if (std::strcmp(name, "table:table") == 0)
        {
            std::vector<std::vector<std::string>> rows = parseOdfTableElement(element);
            if (rows.empty()) continue;
            size_t maxColumns = 0;
            for (const auto &row : rows) maxColumns = std::max(maxColumns, row.size());
            if (maxColumns == 0) continue;
            for (auto &row : rows)
                if (row.size() < maxColumns) row.resize(maxColumns);
            const std::string table = makeMarkdownTable(rows);
            if (!table.empty()) blocks.push_back(table);
        }
        else
        {
            collectOdtElements(element, blocks);
        }
    }
}

static std::string parseOdtContentXml(const std::string &xml)
{
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) return {};
    const auto *root = doc.RootElement();
    if (!root) return {};
    const auto *body = findChildElement(root, "office:body");
    if (!body) return {};
    const auto *textBody = findChildElement(body, "office:text");
    if (!textBody) return {};
    std::vector<std::string> blocks;
    collectOdtElements(textBody, blocks);
    return join(blocks, "\n\n");
}

std::string readOdtText(const std::string &path)
{
    const std::string xml = readZipEntry(path, "content.xml");
    if (xml.empty()) return {};
    return parseOdtContentXml(xml);
}

static void collectOdpText(const tinyxml2::XMLElement *element, std::vector<std::string> &out)
{
    if (!element) return;
    const char *name = element->Name();
    if (name && (std::strcmp(name, "text:p") == 0 || std::strcmp(name, "text:h") == 0))
    {
        const std::string text = odfParagraphText(element);
        if (!text.empty()) out.push_back(text);
    }
    for (auto child = element->FirstChildElement(); child; child = child->NextSiblingElement())
        collectOdpText(child, out);
}

static std::string parseOdpContentXml(const std::string &xml)
{
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) return {};
    const auto *root = doc.RootElement();
    if (!root) return {};
    const auto *body = findChildElement(root, "office:body");
    if (!body) return {};
    const auto *presentation = findChildElement(body, "office:presentation");
    if (!presentation) return {};
    std::vector<std::string> slides;
    int index = 1;
    for (auto page = presentation->FirstChildElement("draw:page"); page; page = page->NextSiblingElement("draw:page"))
    {
        std::vector<std::string> lines;
        collectOdpText(page, lines);
        if (lines.empty()) continue;
        const std::string list = formatMarkdownList(join(lines, "\n"));
        if (list.empty()) continue;
        slides.push_back("## Slide " + std::to_string(index++) + "\n\n" + list);
    }
    return join(slides, "\n\n");
}

std::string readOdpText(const std::string &path)
{
    const std::string xml = readZipEntry(path, "content.xml");
    if (xml.empty()) return {};
    return parseOdpContentXml(xml);
}

static std::string parseOdsContentXml(const std::string &xml)
{
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) return {};
    const auto *root = doc.RootElement();
    if (!root) return {};
    const auto *body = findChildElement(root, "office:body");
    if (!body) return {};
    const auto *spreadsheet = findChildElement(body, "office:spreadsheet");
    if (!spreadsheet) return {};
    std::vector<std::string> sections;
    int sheetIndex = 1;
    for (auto table = spreadsheet->FirstChildElement("table:table"); table; table = table->NextSiblingElement("table:table"))
    {
        std::vector<std::vector<std::string>> rows = parseOdfTableElement(table);
        if (rows.empty()) continue;
        size_t maxColumns = 0;
        for (const auto &row : rows) maxColumns = std::max(maxColumns, row.size());
        if (maxColumns == 0) continue;
        for (auto &row : rows)
            if (row.size() < maxColumns) row.resize(maxColumns);
        const std::string tableMd = makeMarkdownTable(rows);
        if (tableMd.empty()) continue;
        std::string title = table->Attribute("table:name") ? table->Attribute("table:name") : "";
        if (title.empty()) title = "Sheet " + std::to_string(sheetIndex);
        sections.push_back("## " + title + "\n\n" + tableMd);
        ++sheetIndex;
    }
    return join(sections, "\n\n");
}

std::string readOdsText(const std::string &path)
{
    const std::string xml = readZipEntry(path, "content.xml");
    if (xml.empty()) return {};
    return parseOdsContentXml(xml);
}

static std::string collectInlineString(const tinyxml2::XMLElement *element)
{
    std::string text;
    for (auto child = element->FirstChildElement(); child; child = child->NextSiblingElement())
    {
        if (std::strcmp(child->Name(), "t") == 0)
        {
            if (const char *value = child->GetText()) text += value;
        }
        else
        {
            text += collectInlineString(child);
        }
    }
    return trim(text);
}

static std::vector<std::string> parseSharedStringsXml(const std::string &xml)
{
    std::vector<std::string> values;
    if (xml.empty()) return values;
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) return values;
    for (auto entry = doc.RootElement() ? doc.RootElement()->FirstChildElement("si") : nullptr; entry; entry = entry->NextSiblingElement("si"))
    {
        std::string text;
        for (auto child = entry->FirstChildElement(); child; child = child->NextSiblingElement())
        {
            if (std::strcmp(child->Name(), "t") == 0)
            {
                if (const char *value = child->GetText()) text += value;
            }
            else
            {
                text += collectInlineString(child);
            }
        }
        values.push_back(trim(text));
    }
    return values;
}

static std::string readWorksheetCell(const tinyxml2::XMLElement *cell, const std::vector<std::string> &sharedStrings)
{
    const char *type = cell->Attribute("t");
    if (type && std::strcmp(type, "inlineStr") == 0)
    {
        if (const auto *inlineStr = cell->FirstChildElement("is")) return collectInlineString(inlineStr);
        return {};
    }
    if (type && std::strcmp(type, "s") == 0)
    {
        const auto *valueElement = cell->FirstChildElement("v");
        if (!valueElement) return {};
        const int idx = valueElement->IntText(-1);
        if (idx >= 0 && idx < static_cast<int>(sharedStrings.size())) return sharedStrings[idx];
        return valueElement->GetText() ? valueElement->GetText() : "";
    }
    const auto *valueElement = cell->FirstChildElement("v");
    if (!valueElement) return {};
    const char *value = valueElement->GetText();
    return value ? trim(value) : "";
}

static std::vector<std::vector<std::string>> parseWorksheet(const std::string &xml, const std::vector<std::string> &sharedStrings)
{
    std::vector<std::vector<std::string>> rows;
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) return rows;
    const auto *sheetData = doc.RootElement() ? doc.RootElement()->FirstChildElement("sheetData") : nullptr;
    if (!sheetData) return rows;
    for (auto row = sheetData->FirstChildElement("row"); row; row = row->NextSiblingElement("row"))
    {
        std::vector<std::string> cells;
        for (auto cell = row->FirstChildElement("c"); cell; cell = cell->NextSiblingElement("c"))
            cells.push_back(readWorksheetCell(cell, sharedStrings));
        if (!cells.empty()) rows.push_back(cells);
    }
    return rows;
}

static int extractTrailingNumber(const std::string &name)
{
    int i = static_cast<int>(name.size()) - 1;
    while (i >= 0 && !std::isdigit(static_cast<unsigned char>(name[i]))) --i;
    if (i < 0) return 0;
    const int end = i;
    while (i >= 0 && std::isdigit(static_cast<unsigned char>(name[i]))) --i;
    return std::stoi(name.substr(i + 1, end - i));
}

std::string readXlsxText(const std::string &path)
{
    ZipArchive zip;
    if (!zip.open(path)) return {};
    const std::vector<std::string> sharedStrings = parseSharedStringsXml(zip.fileContent("xl/sharedStrings.xml"));
    std::vector<std::string> sheetFiles = zip.filesWithPrefix("xl/worksheets/sheet");
    if (sheetFiles.empty()) return {};
    std::sort(sheetFiles.begin(), sheetFiles.end(), [](const std::string &a, const std::string &b) {
        const int na = extractTrailingNumber(a);
        const int nb = extractTrailingNumber(b);
        if (na == nb) return a < b;
        return na < nb;
    });

    std::vector<std::string> sheets;
    int index = 1;
    for (const std::string &sheetFile : sheetFiles)
    {
        const std::string xml = zip.fileContent(sheetFile);
        if (xml.empty()) continue;
        const auto rows = parseWorksheet(xml, sharedStrings);
        if (rows.empty()) continue;
        const std::string table = makeMarkdownTable(rows);
        if (table.empty()) continue;
        sheets.push_back("## Sheet " + std::to_string(index++) + "\n\n" + table);
    }
    return join(sheets, "\n\n");
}

static void collectParagraphText(const tinyxml2::XMLElement *node, std::string &out)
{
    if (!node) return;
    const char *name = node->Name();
    if (std::strcmp(name, "a:t") == 0)
    {
        if (const char *value = node->GetText()) out += value;
    }
    else if (std::strcmp(name, "a:br") == 0)
    {
        out.push_back('\n');
    }
    for (auto child = node->FirstChildElement(); child; child = child->NextSiblingElement())
        collectParagraphText(child, out);
}

static void collectSlideParagraphs(const tinyxml2::XMLElement *node, std::vector<std::string> &paragraphs)
{
    if (!node) return;
    if (std::strcmp(node->Name(), "a:p") == 0)
    {
        std::string text;
        collectParagraphText(node, text);
        const std::string trimmedText = trim(text);
        if (!trimmedText.empty()) paragraphs.push_back("- " + trimmedText);
        return;
    }
    for (auto child = node->FirstChildElement(); child; child = child->NextSiblingElement())
        collectSlideParagraphs(child, paragraphs);
}

static std::string parseSlideXml(const std::string &xml)
{
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) return {};
    std::vector<std::string> paragraphs;
    collectSlideParagraphs(doc.RootElement(), paragraphs);
    return join(paragraphs, "\n");
}

std::string readPptxText(const std::string &path)
{
    ZipArchive zip;
    if (!zip.open(path)) return {};
    std::vector<std::string> slideFiles = zip.filesWithPrefix("ppt/slides/slide");
    if (slideFiles.empty()) return {};
    std::sort(slideFiles.begin(), slideFiles.end(), [](const std::string &a, const std::string &b) {
        const int na = extractTrailingNumber(a);
        const int nb = extractTrailingNumber(b);
        if (na == nb) return a < b;
        return na < nb;
    });
    std::vector<std::string> slides;
    int index = 1;
    for (const std::string &slideFile : slideFiles)
    {
        const std::string slideXml = zip.fileContent(slideFile);
        if (slideXml.empty()) continue;
        const std::string text = parseSlideXml(slideXml);
        if (text.empty()) continue;
        slides.push_back("## Slide " + std::to_string(index++) + "\n\n" + text);
    }
    return join(slides, "\n\n");
}

} // namespace detail

} // namespace doc2md
