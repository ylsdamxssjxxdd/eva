# doc2md

一个使用纯 C++11 实现的通用 Office/WPS → Markdown 转换器。项目提供 `doc2md::convertFile` API 与命令行工具，可将 Word、PowerPoint、Excel、WPS 套件的现代格式（docx/pptx/xlsx）与二进制格式（doc/wps/dps/et），以及常见纯文本、Markdown、HTML、代码文件统一转换为 Markdown 文本，方便二次编辑或放入知识库。

## 功能概览

- **广泛的输入格式**：`*.txt *.md *.markdown *.html *.htm *.py *.c *.cpp *.cc *.h *.hpp *.json *.js *.ts *.css *.ini *.cfg *.log *.doc *.docx *.odt *.pptx *.odp *.xlsx *.ods *.wps *.et *.dps` 等主流文档、代码与配置文件。
- **结构化转写**：
  - Word/docx：保留段落、标题、嵌套表格并转换为 Markdown 表格或列表。
  - PPTX/DPS：逐页提取幻灯片标题与项目符号，输出 `## Slide N` 区块。
  - XLSX/ET：按工作表生成 `## Sheet N` 段落与 Markdown 表格。
  - 代码/配置/日志等：用 Markdown 代码块包裹，保留语法高亮信息。
- **CLI 与库双形态**：`doc2md_cli` 提供快速批量转换；`doc2md::convertFile` 可嵌入其他程序。
- **跨平台**：依赖 CMake、标准 C++11 与少量第三方库，可在 Windows / Linux 上构建。
- **中文增强**：大量解析逻辑针对中文文档字符集、编码页做了兼容。

## 工作原理与实现细节

### 统一入口
`doc2md::convertFile(path, options)` 会根据扩展名选择解析器，默认执行以下步骤：
1. 将路径与扩展名转为小写，匹配对应的处理器。
2. 对纯文本/代码/Markdown/HTML 文件：直接读取、裁剪或包裹为 Markdown 代码块，并在必要时执行 HTML/Markdown→文本的轻量归一化。
3. 对 Office/WPS 文档：调用专用解析器生成结构化 Markdown。
4. 对未知格式：回退到纯文本读取，并尝试修剪多余空行。

### 现代 Office (docx/pptx/xlsx)
- 使用 **miniz** 以内存方式解压 ZIP 容器，提取 `document.xml`/`presentation.xml`/`sheet*.xml` 等 XML 文件。
- 依赖 **tinyxml2** 解析 XML，逐节点提取段落、文本 run、表格与形状。
- Word：识别 `w:p` 段落样式（Heading1...Heading6）、`w:tbl` 表格，并递归处理嵌套表格。
- PPTX：遍历 `slideN.xml`，收集文本框与项目符号，按幻灯片编号输出 `## Slide N`。
- XLSX：解析行列单元格，补齐列数，用 Markdown 表格呈现。

### WPS/Office 二进制格式 (doc/wps/dps/et)
- 通过 **CompoundFileReader** 读取 OLE 结构（`WordDocument`、`PowerPoint Document`、`Workbook` 等流）。
- Word/WPS (`doc`/`wps`)：
  - 解析 FIB、piece table，重建 UCS-2/ANSI 文本。
  - 特殊处理 0x07/0x0D 控制符，将“表格 tab + 段落”模式转换为 Markdown 表格。
  - 当结构信息缺失时，使用启发式合并连续可打印文本。
- DPS (WPS PowerPoint)：
  - 递归遍历 PPT 记录树，识别 `SlideContainer`，收集 `TextCharsAtom/TextBytesAtom/CString` 文本。
  - 根据幻灯片容器分组，输出 `## Slide N` 列表。若结构不可用，退回普通列表模式。
- ET (WPS Excel)：
  - 优先调用 **libxls** 读取 BIFF 表数据，保留工作表划分。
  - 若 `libxls` 失败，再解析 Workbook 流、检测编码页或使用 UTF-16 扫描。

### 其他文件类型
- **Markdown/HTML**：提供轻量清洗，移除多余标签或 Markdown 语法。
- **代码/脚本/配置**：将全文包裹在 ```lang 代码块中，保留原始缩进。
- **Fallback**：无法识别时读取为 UTF-8 文本，尽量剪裁多余空行，以 Markdown 文本返回。

### 第三方依赖
- [`miniz`](https://github.com/richgel999/miniz)：ZIP 读取。
- [`tinyxml2`](https://github.com/leethomason/tinyxml2)：XML 解析。
- [`libxls`](https://github.com/libxls/libxls)：BIFF/Excel 解析，用于 ET 文件。

## 目录结构

```
include/doc2md/      // 公共 API：document_converter.h
src/doc2md/          // 文档解析与 Markdown 生成核心实现
examples/doc2md_cli/ // CLI 示例程序
thirdparty/          // miniz、tinyxml2、libxls 集成
tests/               // 覆盖 doc/wps/dps/et 等示例文档
docs/                // 项目迭代记录（docs/功能迭代.md）
参考项目/qt-parser  // 原型参考实现，便于对照算法
```

## 构建与安装

### 先决条件
- C++11 编译器（MSVC 2019+/Clang/GCC）。
- [CMake 3.16+](https://cmake.org/)。  
- Windows 上建议使用 VS 开发者命令行；Linux 需确保安装 `build-essential`/`clang` 等工具链。

### 构建示例
```bash
# 生成构建目录
cmake -B build -S .

# 编译库与 CLI
cmake --build build --config Release
```
命令完成后，`build/examples/doc2md_cli[.exe]` 即为可执行程序，静态库位于 `build/src/libdoc2md_lib.a`（或对应平台产物）。

## 命令行使用
```bash
doc2md_cli [-h] [-o output.md] <input-file>
```
- 未指定 `-o` 时，会在当前目录生成 `输入文件名.md`。
- 返回码：`0` 成功，`2` 表示解析器未产生输出（同时打印 warning 信息）。

示例：
```bash
# 将测试文档批量转换到 build/examples/
build/examples/doc2md_cli tests/测试.docx -o build/examples/测试_docx.md
build/examples/doc2md_cli tests/测试.doc  -o build/examples/测试_doc.md
build/examples/doc2md_cli tests/测试.wps  -o build/examples/测试_wps.md
```

## 库集成
```cpp
#include <doc2md/document_converter.h>

using namespace doc2md;

int main() {
    ConversionResult result = convertFile("tests/测试.pptx");
    if (result.success) {
        // result.markdown 即可写文件或送入知识库
    } else {
        for (const auto &warn : result.warnings) {
            // 记录解析过程中的提示
        }
    }
}
```
`ConversionOptions` 提供 `wrapCodeBlocks`、`normalizeWhitespace` 等开关，可按需扩展。

## 测试与示例数据
`tests/` 目录包含一组覆盖 doc/docx/pptx/xlsx/wps/et/dps 的样例文件，可用于本地验证。运行 CLI 后比较生成的 Markdown，即可确认不同格式间的输出是否一致（例如 `测试.doc`、`测试.wps`、`测试.docx` 的表格结果）。

## 迭代记录
重要改动会记录在 `docs/功能迭代.md` 中（中文按时间倒序列出），便于追踪算法演进与问题修复。

## 贡献
欢迎提交 Issue 或 PR：
- 新增或改进文件格式解析。
- 优化 Markdown 输出（段落格式、表格兼容等）。
- 完善跨平台构建与自动化测试。
在提交前请遵循 C++11 代码风格与现有文件组织方式，保持 README/功能迭代文档的同步更新。
