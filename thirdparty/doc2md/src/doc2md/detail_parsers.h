#pragma once

#include <string>

namespace doc2md
{
namespace detail
{
std::string toLower(std::string value);
std::string trim(const std::string &value);

std::string readTextFile(const std::string &path);
std::string markdownToText(const std::string &markdown);
std::string htmlToText(const std::string &html);

std::string readDocxText(const std::string &path);
std::string readOdtText(const std::string &path);
std::string readPptxText(const std::string &path);
std::string readOdpText(const std::string &path);
std::string readXlsxText(const std::string &path);
std::string readOdsText(const std::string &path);
std::string readWpsText(const std::string &path);
std::string readPdfText(const std::string &path);
std::string readEtText(const std::string &path);
std::string readDpsText(const std::string &path);
} // namespace detail
} // namespace doc2md
