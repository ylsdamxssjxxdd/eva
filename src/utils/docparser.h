// docparser.h - Helpers to read various document formats into plain text
// Supports: .txt (UTF-8), .md (strip basic markdown), .docx/.pptx/.xlsx (zip extractor + XML parsing), .doc/.wps (Compound Binary scan)

#pragma once

#include <QString>

namespace DocParser
{
// Read a UTF-8 text file; returns empty on failure
QString readPlainTextFile(const QString &path);

// Very lightweight markdown to plain text: strip code fences, links, images, headings, quotes, tables
QString markdownToText(const QString &md);

// Extract plain text from a .docx file by unpacking document.xml and parsing paragraph runs.
QString readDocxText(const QString &path);

// Extract basic text out of legacy WPS/Word binary documents by scanning UTF-16 spans.
QString readWpsText(const QString &path);

// Extract sheet text (cells joined by tab/line breaks) from an .xlsx file.
QString readXlsxText(const QString &path);

// Extract slide text from a .pptx deck; returns paragraphs grouped by slide.
QString readPptxText(const QString &path);

// Extract text from legacy WPS Excel (.et) documents.
QString readEtText(const QString &path);

// Extract text from legacy WPS PowerPoint (.dps) documents.
QString readDpsText(const QString &path);
} // namespace DocParser
