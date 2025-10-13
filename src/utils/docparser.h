// docparser.h - Helpers to read various document formats into plain text
// Supports: .txt (UTF-8), .md (strip basic markdown), .docx (Windows: PowerShell Expand-Archive)

#pragma once

#include <QString>

namespace DocParser
{
// Read a UTF-8 text file; returns empty on failure
QString readPlainTextFile(const QString &path);

// Very lightweight markdown to plain text: strip code fences, links, images, headings, quotes, tables
QString markdownToText(const QString &md);

// Extract plain text from a .docx file.
// Windows: uses PowerShell Expand-Archive to a temp dir then parses word/document.xml.
// Non-Windows: returns empty string (graceful fallback).
QString readDocxText(const QString &path);
}

