WPS 文档解析
============

1. 解析架构概览  
   当前实现直接在 `src/utils/docparser.cpp` 中构建了一个轻量级的 Compound File Binary (CFB) 读取器 `CompoundFileReader`。该读取器按照微软 CFB 规范完成：
   - 校验文件前 8 字节签名 `0xD0CF11E0A1B11AE1`，确认输入是复合文档/老版 WPS/Word 二进制。  
   - 解析 Sector/MiniSector 大小、FAT/miniFAT/DIFAT 链、根目录等元数据，支持 512B/4KB 等常见扇区尺寸。全流程以 Qt 基础类型和 `qFromLittleEndian` 操作完成，无平台特定 API。  
   - 遍历目录项，定位 `WordDocument` 主流和 `0Table/1Table` 目录，按 WPS/Word 的 `fWhichTblStm` 标志动态选择。

2. Word Binary 结构解析  
   - 通过 FIB (File Information Block) 读取 `fcMin/fcMac`、`fcClx/lcbClx` 等关键偏移，并判定文档是否使用 piece-table。  
   - 若存在 CLX (piece table) 信息，则解析 `CLX -> PLC -> PieceDescriptor`，区分 Unicode/Win1252 压缩块，拼接完整文本。  
   - 若 FIB 指示为 simple document（无复杂结构），则直接在 `WordDocument` 中按照 `fcMin/fcMac` 读取 UTF-16 文本。  
   - 解析完成后会将 `0x01/0x07/0x0D` 等控制符统一转为换行并剔除无意义的 0 字符，最终得到结构化的段落/表格文本。

3. 兼容性说明  
   - 解析实现仅依赖 Qt 提供的 `QFile`, `QString`, `QTextCodec` 等跨平台组件，与平台（Linux/Windows/macOS）无关。  
   - 复合文档访问不依赖系统 API/PowerShell，仅操作内存缓冲，Linux 下无需额外依赖即可使用。  
   - 若未来遇到极端损坏/未知结构，仍保留旧的 UTF-16 扫描 `readWpsHeuristic` 作为兜底，确保解析链不会因个别文件而完全失效。

4. 回退与错误处理  
   - 当复合流缺失或 CLX 数据损坏时，自动回退到 heuristic 模式（原有的 UTF-16 扫描 + 片段过滤），至少保证能抽取普通段落。  
   - 读取链路中所有越界读取都会提前返回空字符串，避免崩溃；调用侧（知识库导入、单测）只需检查返回是否为空。
