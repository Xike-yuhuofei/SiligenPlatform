# process-path contracts

`modules/process-path/contracts/` 承载 `M6 process-path` 的模块 owner 专属契约。

## 正式职责

- 定义工艺路径生成请求与输入约束。
- 固化路径段、路径序列和路径校验结果的模块内语义。
- 固化 `IPathSourcePort` 这类面向外部 consumer 的路径输入 port 契约。
- 作为 `M6 -> M7` 交接的最小字段边界说明。
- `ProcessPathFacade` 的 request/result 与 `IPathSourcePort` 是当前 live contract surface。

## 边界约束

- 仅放置 `M6 process-path` owner 专属契约，不放跨模块稳定公共契约。
- `shared/contracts/engineering/` 仅承载跨模块工程数据 schema / exchange 契约，不承担 `M6` owner 收口。
- `ProcessPath`、`PathGenerationRequest`、`PathGenerationResult`、`IPathSourcePort` 是当前 canonical 契约入口。
- `Siligen::ProcessPath::Contracts` 是唯一 live 语义命名空间；`domain/trajectory/ports/*.h` bridge 与 legacy DXF contract 已退出 public surface。
- `PathGenerationRequest.alignment` 当前只表达 `M5 -> M6` provenance，不表达 M6 内可执行的数值几何变换。
- `PathGenerationRequest.topology_repair.policy` 默认是 `Off`，不再保留旧的 `enable` 裸开关语义。
- `TopologyRepairPolicy::Auto` 只接受带完整 `metadata` 的输入；`metadata.size()` 必须等于 `primitives.size()`。
- `PathGenerationResult.shaped_path` 是对 `M7` 负责的最终路径事实；`process_path` 只作为 M6 内部 pre-shape 对照面。
