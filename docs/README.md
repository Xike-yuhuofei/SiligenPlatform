# Docs

`docs/` 是 `SiligenSuite` 的工作区级文档入口。当前整理口径以 `Codex` 为主要消费者：

- 先读取正式文档区，作为当前真值和默认指引
- 再按需读取历史文档区，作为追溯、复盘和上下文补充
- 不允许把历史材料默认当作当前正式口径

## 1. 消费顺序

当需要理解当前仓库的正式规则、边界和运行方式时，默认按以下顺序读取：

1. `architecture/`
2. `decisions/`
3. `machine-model/`
4. `runtime/`
5. `validation/`
6. `onboarding/`

只有在正式文档不足以解释当前问题时，才继续读取历史文档区。

## 2. 正式文档区

以下目录默认属于正式文档区。除非目录内文档显式声明为历史记录，否则应优先视为当前口径：

- `architecture/`：工作区架构、目录职责、依赖规则、迁移原则、正式系统链路说明
- `decisions/`：工作区级 ADR、关键治理决策和边界冻结结论
- `machine-model/`：机型级硬件事实、接线、IO 映射、联机约束和现场复核结论
- `runtime/`：部署、发布、回滚、现场验收和运行期流程
- `validation/`：正式验证分层、发布测试入口、测试矩阵与验收口径
- `onboarding/`：开发者入场、默认工作方式、协作约定
- `troubleshooting/`：当前仍有效的故障排查手册；若文档仅适用于历史阶段，必须显式标注

## 3. 历史文档区

以下目录默认属于历史文档区。它们保留追溯价值，但不应被 `Codex` 默认视为当前正式口径：

- `process-model/`：任务规划、评审、复盘等过程材料；默认服务于追溯与 closeout
- `session-handoffs/`：会话交接记录，仅用于续接上下文
- `_archive/`：已归档的旧方案、旧阶段产物和退出材料
- `architecture-audits/`：阶段性审计、盘点、专项分析和重构过程证据；除非被正式文档引用为当前权威结论，否则按历史材料处理

## 4. 使用规则

对 `Codex` 的默认要求：

1. 不要把 `plan`、`review`、`retro`、`handoff` 直接当作当前正式规范
2. 如果正式区与历史区表述冲突，以正式区为准
3. 如果历史文档中有仍然有效的结论，应把该结论上收至正式区，而不是继续让历史材料承担真值职责
4. 若某个目录缺少 README 或边界声明，应优先补入口，不要先扩散新文档

## 5. 当前目录导航

### 正式文档区

- [architecture/](./architecture/README.md)
  当前优先入口：`legacy-cutover-status.md`、`system-acceptance-report.md`、`performance-baseline.md`、`dxf-motion-execution-code-map-v1.md`
- [decisions/](./decisions/README.md)
  当前优先入口：`ADR-002-canonical-workflow.md`、`ADR-003-versioning-and-release.md`、`ADR-005-cmp-runtime-config-boundary.md`、`job-ingest-upload-semantic-freeze.md`、`process-path-legacy-compat-freeze.md`
- [machine-model/](./machine-model/README.md)
- [runtime/](./runtime/README.md)
- [validation/](./validation/README.md)
- [onboarding/](./onboarding/README.md)
- [troubleshooting/](./troubleshooting/README.md)

### 历史文档区

- [process-model/](./process-model/README.md)
- [session-handoffs/](./session-handoffs/README.md)
- [_archive/](./_archive/README.md)
- [architecture-audits/](./architecture-audits/README.md)

## 6. 新增文档落位规则

新增文档前，先判断它属于哪一种：

- 当前正式规则、接口、约束、运行手册：放正式文档区
- 任务过程产物、评审记录、阶段报告、交接材料：放历史文档区
- 若内容同时包含正式结论和过程噪音：拆分为“正式结论文档 + 历史过程附件”，不要混写

命名与目录职责冲突时，以“是否承担当前真值职责”为最高判定标准。
