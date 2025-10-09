# 记忆进度条与速度显示（kv_bar & speed）

本文档说明 EVA 如何从 llama.cpp server 的输出与 xNet 流式数据中推导出“记忆进度条（KV 缓存使用量）”与“文字生成速度/上文处理速度”的显示逻辑。

## 名词与日志来源

- 记忆量（KV used）：等价于当前会话 slot 的 `n_past`（上下文缓存中的 token 数目）。
- 最大记忆容量：`n_ctx_slot`（通常接近用户设置的 `n_ctx / parallel`）。
- server 侧输出（本地模式）：来自 `llama.cpp/tools/server` 的 stdout/stderr，例如：
  - `slot update_slots: ... n_ctx_slot = 4096`
  - `kv cache rm [X, end)`（方括号中的第一个数字 X）
  - `prompt done, n_past = N, n_tokens = M`
  - `stop processing: n_past = K, truncated = 0`
  - `slot print_timing: ... prompt eval time = ... tokens per second`
  - `slot print_timing: ... eval time = ... tokens per second`
  - `srv  update_slots: all slots are idle`
- xNet 流式输出（链接模式或兜底）：在没有本地 server 日志时，按接收到的内容块累计 token 数作为近似（每个 SSE 块视作 1 token）。

## kv_bar 的更新规则（记忆进度条）

- 文本：显示为 `记忆:xx%`（中文冒号）；
- 进度：使用第二种（黄色）进度条显示当前百分比；第一段（蓝色）固定为 0。
- 工具提示：`上下文缓存量 {used} / {max} token`。
- 当捕捉到 `n_ctx_slot = S` 时，记录 S 为最大记忆容量。
  - 若未捕捉到，则以 UI 的 `n_ctx`（或默认值）作为兜底。
- 当捕捉到 `kv cache rm [X, end)` 时：将当前记忆量修正为 `X`（命中缓存的 token 数）。
- 当捕捉到 `prompt done, n_past = N` 或 `prompt processing progress, n_past = N` 时：将当前记忆量修正为 `N`。
- 当捕捉到 `stop processing: n_past = K` 时：将当前记忆量修正为 `K`。
- 链接模式（无本地 server 输出）下：按 xNet 收到的流式 token 数在本轮基础上累加，近似更新当前记忆量（本轮开始时的记忆量 + 流式 token 数）。

实现位置：
- UI 侧：`Widget::onServerOutput()` 解析 server 行为，并即时 `updateKvBarUi()`；`Widget::recv_kv_from_net()` 在流式期间近似更新。
- 进度条控件：`ui->kv_bar`，只设置 `value`（百分比）与 `toolTip`。

优先级与差异说明：
- 最终以 `stop processing: n_past = K` 为权威矫正值；
- `total` 与 `n_past` 可能差 1 或数十：常见于 BOS/EOS 计数、`n_keep`、缓存命中清理（`kv cache rm`）、或不同版本的日志口径差异。为了用户可理解性，我们始终以 `n_past` 为主。

## 速度显示（状态区）

- 上文处理速度：`prompt eval time` 行中的 tokens per second；若没捕捉到，显示 `--`。
- 文字生成速度：`eval time` 行中的 tokens per second；
  - 若没捕捉到，则使用“流式 token 数 / 本轮用时”作为兜底。
- 统一在本轮结束时以“一行”输出（形如：`ui: 文字生成 {X} tokens/s  上文处理 {Y} tokens/s`）。
- 本地模式：捕捉到 `all slots are idle` 后输出；装载后的第一次 `all slots are idle` 仅表示服务端空闲态，忽略不处理。
- 链接模式：仅输出“文字生成速度”，`上文处理` 固定为 `--`。

实现位置：
- 解析：`Widget::onServerOutput()` 通过正则提取 `prompt eval time` 与 `eval time` 的 tokens/s，缓存于 `lastPromptTps_` / `lastGenTps_`；
- 完成时输出：
  - 本地：捕捉 `all slots are idle` 后，先用解析值输出；若生成速度缺失，fallback 为“流式 token 数 / 本轮用时”。
  - 链接：`Widget::recv_pushover()` 中仅计算并输出“文字生成速度”。

## 关键细节与边界情况

- `n_ctx_slot` 更新即刻生效，用于后续进度条换算；若未获取则回退到 `n_ctx`。
- `kv cache rm [X, end)` 的 `X` 视为“命中缓存的 token 数”，用于矫正刚发完响应时的记忆量（避免简单累加导致偏大）。
- `prompt done` 的 `n_past`、`stop processing` 的 `n_past` 都用于强纠正当前记忆量（权威值）。
- 在本轮等待 `stop processing` 之前，按 xNet 流式 token 数持续近似累增记忆量，并据此计算兜底的生成速度。
- 若既没有 `prompt eval time` 也没有 `eval time`，则仅用兜底的生成速度；上文处理速度显示 `--`。

## 代码入口

- 日志解析：`src/widget/widget_slots.cpp` 中 `Widget::onServerOutput()`。
- 流式兜底：
  - `src/xnet.cpp` 每收到一个内容块 `emit net2ui_kv_tokens(tokens_)`；
  - `src/main.cpp` 连接到 `Widget::recv_kv_from_net()`；
  - `src/widget/widget_slots.cpp` 在该槽函数内累积并刷新 `kv_bar`。
- 回合收尾：
  - 本地：`onServerOutput()` 捕捉 `all slots are idle` 后输出速度；
  - 链接：`src/widget/widget.cpp` 的 `Widget::recv_pushover()` 输出生成速度。

以上实现满足：
- 记忆进度条随模型输出动态更新；
- 本地模式精确修正；
- 链接模式提供稳定的近似与速度兜底；
- 日志与流式信息缺失时也能给出合理显示。
