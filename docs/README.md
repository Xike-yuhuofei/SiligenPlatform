# Docs

`docs/` 是 `SiligenSuite` 的工作区级文档入口。

- 先读取正式文档区，作为当前真值和默认指引
- 再按需读取历史文档区，作为追溯、复盘和上下文补充
- 不允许把历史材料默认当作当前正式口径

完整的文档整理方案详见 [docs-organization-plan.md](./docs-organization-plan.md)。

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

| 目录 | 职责 |
|------|------|
| `architecture/` | 工作区架构、目录职责、依赖规则、迁移原则、正式系统链路说明 |
| `decisions/` | 工作区级 ADR、关键治理决策和边界冻结结论 |
| `machine-model/` | 机型级硬件事实、接线、IO 映射、联机约束和现场复核结论 |
| `runtime/` | 部署、发布、回滚、现场验收和运行期流程 |
| `validation/` | 正式验证分层、发布测试入口、测试矩阵与验收口径 |
| `onboarding/` | 开发者入场、默认工作方式、协作约定 |
| `troubleshooting/` | 当前仍有效的故障排查手册 |

## 3. 历史文档区

以下目录默认属于历史文档区。它们保留追溯价值，但不应被默认视为当前正式口径：

| 目录 | 职责 |
|------|------|
| `process-model/` | 任务规划、评审、复盘等过程材料（已精简，仅保留汇总版）；批量操作计划和多版本评审已移入 `_archive/process-model/` |
| `session-handoffs/` | 会话交接记录，仅用于续接上下文 |
| `_archive/` | 已归档的旧方案、旧阶段产物和退出材料；含 `process-model/` 精简移出的文件 |
| `architecture/history/audits/architecture-audits/` | 从顶层合并而来的专项审计包（原 `architecture-audits/`） |

## 4. 目录结构总览

```
docs/
├── README.md                    # 本文件 — 总入口
├── docs-organization-plan.md    # 文档整理方案
│
├── architecture/                # [正式] 架构文档
├── decisions/                   # [正式] 决策记录
├── machine-model/               # [正式] 机型事实
├── onboarding/                  # [正式] 开发者指引
├── runtime/                     # [正式] 运行手册
├── troubleshooting/             # [正式] 故障排查
├── validation/                  # [正式] 验证体系
│
├── process-model/               # [历史] 过程记录（精简版）
├── _archive/                    # [历史] 归档材料
│
└── assets/                      # [共享] 全局资产
    └── images/                  #       全局共用图片
```

## 5. 使用规则

1. 不要把 `plan`、`review`、`retro`、`handoff` 直接当作当前正式规范
2. 如果正式区与历史区表述冲突，以正式区为准
3. 如果历史文档中有仍然有效的结论，应把该结论上收至正式区，而不是继续让历史材料承担真值职责
4. 若某个目录缺少 README 或边界声明，应优先补入口，不要先扩散新文档

## 6. 新增文档落位规则

新增文档前，先通过以下检查：

```
□ 确定文档类型（正式/历史）
□ 确定存放目录（根据职责表）
□ 文件名使用小写 kebab-case，见命名规范
□ 是否需要在目标目录 README 中补充入口链接
□ 是否已有同主题文档（避免重复）
□ 若包含正式结论，确保放入正式区而非历史区
```

命名与目录职责冲突时，以"是否承担当前真值职责"为最高判定标准。

## 7. 维护机制

- **容量警戒**：`process-model/` 文件数超过 30 时应主动归档
- **定期审查**：每次 PR 审查新增文档合规性；每月检查正式区过时文档
- **版本控制**：正式规范使用 `-v1`、`-v2` 后缀；冻结文档集在目录 README 声明
- **引用保护**：调整文档位置时必须更新所有外部引用

详细规范请参考 [docs-organization-plan.md](./docs-organization-plan.md)。
