// docparser.h - Helpers to read various document formats into plain text
// Supports: .txt (UTF-8), .md (strip basic markdown), .docx (zip extractor + XML parsing), .wps (heuristic UTF-16 scan)

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
} // namespace DocParser
