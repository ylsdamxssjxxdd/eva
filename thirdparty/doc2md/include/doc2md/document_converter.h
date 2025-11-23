#pragma once

#include <string>
#include <vector>

namespace doc2md
{
struct ConversionOptions
{
    bool wrapCodeBlocks = true;
    bool normalizeWhitespace = true;
};

struct ConversionResult
{
    std::string markdown;
    std::vector<std::string> warnings;
    bool success = false;
};

ConversionResult convertFile(const std::string &path, const ConversionOptions &options = ConversionOptions());
} // namespace doc2md
