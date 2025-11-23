#include "doc2md/document_converter.h"

#include <fstream>
#include <iostream>
#include <string>

namespace
{
void printHelp()
{
    std::cout << "Usage: doc2md [-h] [-o <output.md>] <input>\n"
                 "  -h            Show this help message\n"
                 "  -o <path>     Write converted markdown to the specified file.\n"
                 "                Defaults to <input_basename>.md in the current directory.\n";
}

std::string defaultOutputPath(const std::string &inputPath)
{
    std::string name = inputPath;
    const std::string::size_type slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    const std::string::size_type dot = name.find_last_of('.');
    if (dot != std::string::npos && dot != 0) name = name.substr(0, dot);
    if (name.empty()) name = "output";
    return name + ".md";
}
} // namespace

int main(int argc, char **argv)
{
    std::string inputPath;
    std::string outputPath;
    bool showHelp = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            showHelp = true;
        }
        else if (arg == "-o")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Missing path after -o\n";
                return 1;
            }
            outputPath = argv[++i];
        }
        else if (arg.size() && arg[0] == '-')
        {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
        else
        {
            inputPath = arg;
        }
    }

    if (showHelp)
    {
        printHelp();
        return 0;
    }

    if (inputPath.empty())
    {
        printHelp();
        return 1;
    }

    if (outputPath.empty()) outputPath = defaultOutputPath(inputPath);

    doc2md::ConversionResult result = doc2md::convertFile(inputPath);
    if (!result.success)
    {
        std::cerr << "Failed to convert file: " << inputPath << "\n";
        for (const std::string &warning : result.warnings) std::cerr << "  " << warning << "\n";
        return 2;
    }

    std::ofstream out(outputPath.c_str(), std::ios::binary);
    if (!out)
    {
        std::cerr << "Unable to open output file: " << outputPath << "\n";
        return 3;
    }
    out << result.markdown;
    out.close();
    std::cout << "Wrote Markdown to " << outputPath << "\n";
    return 0;
}
