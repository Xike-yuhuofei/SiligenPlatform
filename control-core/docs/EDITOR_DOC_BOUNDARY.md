# DXF Editor 文档边界说明

## 筛查结论

本轮已对 `control-core/docs` 中包含以下特征的文档进行了筛查：

- `editor`
- `preview`
- `svg`
- `canvas`
- `notify`
- `readonly`
- `autosave`

结论是：**未发现需要从 `control-core` 继续迁移到 `dxf-editor` 的明确文档真源**。

## 原因

`dxf-editor` 的核心文档目前主要来自工具目录本身，已经在首轮迁移时进入 `dxf-editor`：

- `docs/integration/INTEGRATION.md`
- `docs/developer-guide/SPEC.md`
- `docs/developer-guide/SPEC_DISPENSING.md`

而 `control-core/docs` 中当前涉及 DXF 的剩余文档，大多属于以下类型：

- DXF 文件驱动的执行链路与点胶流程
- HMI / TCP UseCase 的运行方式
- 实时控制、硬件响应与架构修复记录
- 轨迹规划预览能力（面向执行/规划结果，而非编辑器文档模型）

这些内容更适合继续保留在 `control-core`，或已在前一轮迁移到 `dxf-pipeline`。

## 当前保留在 control-core 的典型文档

- `docs/plans/2026-01-09-dxf-cli-dispensing-design.md`
- `docs/plans/dxfwebplanning-verification-summary.md`
- `docs/debug/dxf_dispense_axis_response_issue.md`
- `docs/_archive/2026-01/legacy-06-user-guides/dxf-dispensing-guide.md`

## 后续原则

只有当文档明确讨论以下主题时，才优先归属 `dxf-editor`：

- 本地 DXF 编辑器的打开/保存/另存/只读/自动保存
- notify 文件协议
- 编辑器文档模型、画布交互、图元编辑、约束系统
- 编辑器 UI 行为与独立桌面进程集成
