#ifndef TOOL_EXECUTOR_H
#define TOOL_EXECUTOR_H

#include "xtool.h"

// 工具执行器：对 xTool 的封装，方便后续抽离工具注册与执行链路
class ToolExecutor : public xTool
{
    Q_OBJECT
  public:
    explicit ToolExecutor(const QString &applicationDirPath = QStringLiteral("./"))
        : xTool(applicationDirPath)
    {
    }
};

#endif // TOOL_EXECUTOR_H
