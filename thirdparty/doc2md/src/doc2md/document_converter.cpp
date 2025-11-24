#include "doc2md/document_converter.h"
#include "doc2md/detail_parsers.h"

#include <unordered_set>

namespace doc2md
{
ConversionResult convertFile(const std::string &path, const ConversionOptions &options)
{
    (void)options;
    ConversionResult result;
    const std::string lowerPath = detail::toLower(path);
    const auto dot = lowerPath.find_last_of('.');
    const std::string extension = dot == std::string::npos ? "" : lowerPath.substr(dot);

    const std::unordered_set<std::string> plainExtensions = {
        ".txt", ".log", ".json", ".ini", ".cfg"};
    const std::unordered_set<std::string> markdownExtensions = {
        ".md", ".markdown"};
    const std::unordered_set<std::string> codeExtensions = {
        ".cpp", ".cc", ".c", ".h", ".hpp", ".py", ".js", ".ts", ".css", ".html", ".htm"};

    if (extension == ".docx")
        result.markdown = detail::readDocxText(path);
    else if (extension == ".odt")
        result.markdown = detail::readOdtText(path);
    else if (extension == ".pptx")
        result.markdown = detail::readPptxText(path);
    else if (extension == ".odp")
        result.markdown = detail::readOdpText(path);
    else if (extension == ".xlsx")
        result.markdown = detail::readXlsxText(path);
    else if (extension == ".ods")
        result.markdown = detail::readOdsText(path);
    else if (extension == ".doc" || extension == ".wps")
        result.markdown = detail::readWpsText(path);
    else if (extension == ".pdf")
        result.markdown = detail::readPdfText(path);
    else if (extension == ".et")
        result.markdown = detail::readEtText(path);
    else if (extension == ".dps")
        result.markdown = detail::readDpsText(path);
    else if (plainExtensions.count(extension))
        result.markdown = detail::trim(detail::readTextFile(path));
    else if (codeExtensions.count(extension))
    {
        const std::string text = detail::readTextFile(path);
        if (!text.empty())
            result.markdown = "```" + (extension.size() > 1 ? extension.substr(1) : std::string()) + "\n" + text + "\n```";
    }
    else
    {
        result.markdown = detail::trim(detail::readTextFile(path));
    }

    if (markdownExtensions.count(extension))
        result.markdown = detail::markdownToText(result.markdown);
    else if (extension == ".html" || extension == ".htm")
        result.markdown = detail::htmlToText(result.markdown);

    result.success = !result.markdown.empty();
    if (!result.success)
        result.warnings.push_back("No parser produced output for file: " + path);
    return result;
}

} // namespace doc2md
