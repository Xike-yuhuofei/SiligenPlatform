# job-ingest

`M1 job-ingest`

## 模块职责

- 承接 `S0-S1`，创建任务上下文、接收源文件、封存输入并做格式校验。

## Owner 对象

- `JobDefinition`
- `SourceDrawing`

## 输入契约

- `CreateJob`
- `AttachSourceFile`
- `ValidateSourceFile`
- `GetSourceDrawing`

## 输出契约

- `JobCreated`
- `SourceFileAccepted`
- `SourceFileRejected`

## 禁止越权边界

- 不得解析 DXF 为 `CanonicalGeometry`。
- 不得提取特征或绑定工艺细节。
- 不得改写设备运行状态。

## 测试入口

- `S09` success/file reject/corrupted input
