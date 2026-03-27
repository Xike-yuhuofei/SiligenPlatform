# trace-diagnostics

`M10 trace-diagnostics`

## 模块职责

- 承接 `S16`，固化执行事实、归档、追溯查询和故障摘要。

## Owner 对象

- `ExecutionRecord`
- `TraceLinkSet`
- `FaultDigest`
- `QualitySummary`

## 输入契约

- `RecordExecution`
- `AppendFaultEvent`
- `FinalizeExecution`
- `ArchiveWorkflow`
- `QueryTrace`
- `SupersedeOwnedArtifact`

## 输出契约

- `FaultEventAppended`
- `ExecutionRecordBuilt`
- `WorkflowArchived`
- `TraceWriteFailed`
- `TraceQueryResult`
- `ArtifactSuperseded`

## 禁止越权边界

- 不得改写上游对象。
- 不得自动修复工艺计划。
- 不得伪装成实时控制链。

## 测试入口

- `S09` archive-trace-case
