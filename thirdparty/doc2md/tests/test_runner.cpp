#include "doc2md/document_converter.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

namespace
{
std::string appendPath(const std::string &dir, const std::string &name)
{
    if (dir.empty()) return name;
    const char last = dir.back();
    if (last == '/' || last == '\\')
        return dir + name;
    return dir + "/" + name;
}

std::vector<std::string> listTestFiles(const std::string &directory)
{
    std::vector<std::string> files;
#ifdef _WIN32
    std::string native = directory;
    std::replace(native.begin(), native.end(), '/', '\\');
    if (!native.empty() && native.back() != '\\') native.push_back('\\');
    const std::string pattern = native + "*";
    WIN32_FIND_DATAA entry{};
    HANDLE handle = FindFirstFileA(pattern.c_str(), &entry);
    if (handle == INVALID_HANDLE_VALUE) return files;
    do
    {
        if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            files.push_back(appendPath(directory, entry.cFileName));
    } while (FindNextFileA(handle, &entry));
    FindClose(handle);
#else
    DIR *dirp = opendir(directory.c_str());
    if (!dirp) return files;
    while (dirent *entry = readdir(dirp))
    {
        const std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        const std::string fullPath = appendPath(directory, name);
        struct stat info
        {
        };
        if (stat(fullPath.c_str(), &info) != 0) continue;
        if (!S_ISREG(info.st_mode)) continue;
        files.push_back(fullPath);
    }
    closedir(dirp);
#endif
    std::sort(files.begin(), files.end());
    return files;
}

std::string fileExtension(const std::string &path)
{
    const std::string::size_type dot = path.find_last_of('.');
    if (dot == std::string::npos) return {};
    std::string ext = path.substr(dot);
    for (char &ch : ext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return ext;
}

std::string fileNameOnly(const std::string &path)
{
    const std::string::size_type slash = path.find_last_of("/\\");
    return (slash == std::string::npos) ? path : path.substr(slash + 1);
}
} // namespace

int main()
{
    const std::string dataDir = DOC2MD_TEST_DATA_DIR;
    const std::vector<std::string> files = listTestFiles(dataDir);
    if (files.empty())
    {
        std::cerr << "No test files were found in " << dataDir << '\n';
        return 1;
    }

    const std::unordered_map<std::string, std::vector<std::string>> expectedByExtension = {
        {".doc", {"| 1 | 1 | 3 | 3 | 4 |", "EVA_MODELS/llm"}},
        {".docx", {"[English](README_en.md)", "https://pan.baidu.com/s/18NOUMjaJIZsV_Z0toOzGBg?pwd=body"}},
        {".wps", {"EVA_MODELS/speech2text", "Agent"}},
        {".pptx", {"https://hf-mirror.com/", "llama-server.exe"}},
        {".dps", {"https://hf-mirror.com/", "___PPT10"}},
        {".xlsx", {"## Sheet 1", "QML_IMPORT_NAME = \"io.qt.textproperties\""}},
        {".et", {"## Sheet 1", "pyside6-rcc style.qrc -o style_rc.py"}}};
    std::unordered_map<std::string, bool> seenExtensions;
    for (const auto &entry : expectedByExtension) seenExtensions[entry.first] = false;

    bool allPassed = true;
    size_t checkedFiles = 0;
    for (const std::string &path : files)
    {
        const std::string ext = fileExtension(path);
        const auto expectation = expectedByExtension.find(ext);
        if (expectation == expectedByExtension.end())
        {
            std::cout << "[SKIP] " << fileNameOnly(path) << " (no expectations)\n";
            continue;
        }

        seenExtensions[ext] = true;
        ++checkedFiles;
        const std::string baseName = fileNameOnly(path);

        std::ifstream probe(path.c_str(), std::ios::binary);
        if (!probe)
        {
            allPassed = false;
            std::cerr << "[FAIL] " << baseName << " (missing test document)\n";
            continue;
        }

        const doc2md::ConversionResult result = doc2md::convertFile(path);
        if (!result.success)
        {
            allPassed = false;
            std::cerr << "[FAIL] " << baseName << '\n';
            for (const std::string &warning : result.warnings)
                std::cerr << "  warning: " << warning << '\n';
            continue;
        }

        bool contentOk = true;
        for (const std::string &snippet : expectation->second)
        {
            if (result.markdown.find(snippet) == std::string::npos)
            {
                contentOk = false;
                std::cerr << "  missing snippet: \"" << snippet << "\"\n";
            }
        }

        if (!contentOk)
        {
            allPassed = false;
            std::cerr << "[FAIL] " << baseName << " content mismatch\n";
        }
        else
        {
            std::cout << "[ OK ] " << baseName << " => " << result.markdown.size() << " bytes\n";
        }

        for (const std::string &warning : result.warnings)
            std::cout << "  warning: " << warning << '\n';
    }

    for (const auto &entry : seenExtensions)
    {
        if (!entry.second)
        {
            allPassed = false;
            std::cerr << "Missing test document for extension " << entry.first << '\n';
        }
    }

    if (checkedFiles == 0)
    {
        std::cerr << "No files matched the expected extensions.\n";
        return 1;
    }

    if (!allPassed)
    {
        std::cerr << "Some documents failed to convert or did not contain expected content.\n";
        return 1;
    }

    std::cout << "All " << checkedFiles << " test documents passed content verification.\n";
    return 0;
}
