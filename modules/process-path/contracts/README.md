# process-path contracts

`modules/process-path/contracts/` 承载 `M6 process-path` 的模块 owner 专属契约。

## 正式职责

- 定义工艺路径生成请求与输入约束。
- 固化路径段、路径序列和路径校验结果的模块内语义。
- 固化 `IPathSourcePort` / `IDXFPathSourcePort` 这类面向外部 consumer 的路径输入 port 契约。
- 作为 `M6 -> M7` 交接的最小字段边界说明。
- `ProcessPathFacade` 的 request/result 是当前唯一 public contract。

## 边界约束

- 仅放置 `M6 process-path` owner 专属契约，不放跨模块稳定公共契约。
- `shared/contracts/engineering/` 仅承载工程数据 compat/schema，不承担 `M6` owner 收口。
- `ProcessPath`、`PathGenerationRequest`、`PathGenerationResult` 是当前 canonical 契约入口。
- 历史 `domain/trajectory/ports/*.h` 仅保留兼容桥接职责，新的 public include 应优先走 `process_path/contracts/*.h`。
