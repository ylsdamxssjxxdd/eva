// Local llama.cpp server argument builder
#include "xbackend_args.h"

#include "utils/pathutil.h"

namespace
{
QString sanitizedHost(const QString &host)
{
    const QString trimmed = host.trimmed();
    if (trimmed.isEmpty()) return QStringLiteral("0.0.0.0");
    return trimmed;
}

QString sanitizedPort(const QString &port)
{
    const QString trimmed = port.trimmed();
    if (trimmed.isEmpty()) return QStringLiteral(DEFAULT_SERVER_PORT);
    return trimmed;
}
} // namespace

QStringList buildLocalServerArgs(const LocalServerArgsInput &input)
{
    QStringList args;
    if (!input.modelPath.isEmpty())
    {
        args << QStringLiteral("-m") << ensureToolFriendlyFilePath(input.modelPath);
    }

    args << QStringLiteral("--host") << sanitizedHost(input.host);
    args << QStringLiteral("--port") << sanitizedPort(input.port);

    // 默认开启 llama-server 的可选端点：
    // - `/metrics`：Prometheus 兼容指标（便于监控推理吞吐/耗时等）
    // - `/props`：运行时属性读取（EVA 会用它探测 n_ctx/model_alias；同时也支持按需写入）
    // 说明：llama-server 默认关闭这些端点，因此机体需要主动开启以保证探测逻辑稳定可用。
    if (DEFAULT_LLAMA_ENDPOINT_METRICS)
    {
        args << QStringLiteral("--metrics");
    }
    if (DEFAULT_LLAMA_ENDPOINT_PROPS)
    {
        args << QStringLiteral("--props");
    }

    // 默认关闭 kv_unified：避免 llama-server 自动调整并发，保持并发数量完全由 `--parallel` 控制。
    if (DEFAULT_LLAMA_DISABLE_KV_UNIFIED)
    {
        args << QStringLiteral("-kvu");
    }

    const int slotCtx = (input.settings.nctx > 0) ? input.settings.nctx : DEFAULT_NCTX;
    const int parallel = (input.settings.hid_parallel > 0) ? input.settings.hid_parallel : 1;
    const int totalCtx = slotCtx * parallel;
    args << QStringLiteral("-c") << QString::number(totalCtx);

    const QString resolved = input.resolvedDevice.trimmed().toLower();
    if (resolved != QStringLiteral("cpu"))
    {
        args << QStringLiteral("-ngl") << QString::number(input.settings.ngl);
    }

    args << QStringLiteral("--threads") << QString::number(input.settings.nthread);
    args << QStringLiteral("-b") << QString::number(input.settings.hid_batch);
    args << QStringLiteral("--parallel") << QString::number(parallel);
    args << QStringLiteral("--jinja");
    args << QStringLiteral("--reasoning-format") << QStringLiteral("auto");
    args << QStringLiteral("--verbose-prompt");

    if (!input.loraPath.isEmpty())
    {
        args << QStringLiteral("--lora") << ensureToolFriendlyFilePath(input.loraPath);
    }
    if (!input.mmprojPath.isEmpty())
    {
        args << QStringLiteral("--mmproj") << ensureToolFriendlyFilePath(input.mmprojPath);
    }
    if (!input.settings.hid_flash_attn)
    {
        args << QStringLiteral("-fa") << QStringLiteral("off");
    }
    if (input.settings.hid_use_mlock)
    {
        args << QStringLiteral("--mlock");
    }
    if (!input.settings.hid_use_mmap)
    {
        args << QStringLiteral("--no-mmap");// 默认应用--no-mmapno-mmap
    }

    const QString normalizedModel = input.modelPath.toLower();
    const bool modelIsQ40 = normalizedModel.contains(QStringLiteral("q4_0"));
    if (input.win7Backend && modelIsQ40)
    {
        args << QStringLiteral("--no-repack");
    }

    return args;
}
