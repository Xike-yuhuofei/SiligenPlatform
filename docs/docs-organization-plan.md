# Docs 文档整理方案

> 制定日期：2026-04-25
> 状态：待执行
> 执行人：Codex Agent

---

## 1. 现状评估

### 1.1 总量统计

| 区域 | 目录 | 文件数 | 占比 |
|------|------|--------|------|
| 正式文档区 | architecture/ + decisions/ + machine-model/ + runtime/ + validation/ + onboarding/ + troubleshooting/ | ~80 | ~36% |
| 历史文档区 | process-model/ + _archive/ + architecture-audits/ | ~130 | ~59% |
| README 入口 | 各目录 README.md | ~22 | ~5% |
| **合计** | | **~220+** | **100%** |

### 1.2 核心问题清单

| # | 问题 | 严重程度 | 影响范围 |
|---|------|----------|----------|
| P1 | process-model/plans/ 存放约 60 个时间戳临时批次计划，绝大多数已无追溯价值 | 高 | 检索效率、目录膨胀 |
| P2 | process-model/reviews/ 存放约 60 个评审记录，同一特性的多个时间戳版本未合并 | 高 | 检索效率、信息冗余 |
| P3 | architecture-audits/ 与 architecture/history/audits/ 功能重叠 | 高 | 职责模糊、重复存放 |
| P4 | _archive/2026-03/hmi-client/ 层级过深（5层），图片与文档混放 | 中 | 导航困难、管理不便 |
| P5 | 文件名中英文混用、时间戳格式不统一 | 中 | 一致性差、工具处理不便 |
| P6 | decisions/history/legacy-cleanup/ 含有多个版本 cleanup 计划 | 中 | 信息冗余 |
| P7 | 缺少文档命名规范的形式化规则 | 低 | 新文档质量参差 |
| P8 | 缺少过时文档的自动淘汰机制 | 低 | 长期维护隐患 |

---

## 2. 文档分类标准

### 2.1 一级分类：按职能域划分

| 分类 | 目录 | 职责 | 是否当前真值 |
|------|------|------|-------------|
| 架构文档 | `architecture/` | 工作区架构、目录职责、依赖规则、模块边界 | 是 |
| 决策记录 | `decisions/` | ADR、关键治理决策、边界冻结结论 | 是 |
| 机型事实 | `machine-model/` | 硬件接线、IO映射、联机约束 | 是 |
| 运行手册 | `runtime/` | 部署、发布、回滚、现场验收 | 是 |
| 验证体系 | `validation/` | 测试分层、测试矩阵、验收口径 | 是 |
| 开发者指引 | `onboarding/` | 入场引导、工作流约定 | 是 |
| 故障排查 | `troubleshooting/` | 排障手册、标准 runbook | 是 |
| 过程记录 | `process-model/` | 任务规划、评审、复盘 | 否 |
| 归档材料 | `_archive/` | 已退出的旧方案、旧阶段产物 | 否 |

### 2.2 二级分类：按文档类型划分

| 文档类型 | 后缀约定 | 存放位置 | 示例 |
|----------|----------|----------|------|
| 基线规范 | `*-baseline.md`、`*-v1.md` | 所属正式区 | `workspace-baseline.md` |
| 决策记录 | `ADR-NNN-title.md` | `decisions/` | `ADR-001-workspace-baseline.md` |
| 方案设计 | `*-design.md`、`*-proposal.md` | 所属正式区 / `_archive/` | `hmi-tcp-adapter-design.md` |
| 评审记录 | `*-review-*.md` | `process-model/reviews/` | `modules-review.md` |
| 审计报告 | `*-audit.md` | `architecture/history/audits/` | `alias-consumer-audit.md` |
| 复盘总结 | `*-retro.md` | `process-model/retro/` | 暂无 |
| 验收报告 | `*-acceptance-*.md` | `validation/` | `system-acceptance-report.md` |
| 模板文件 | `*-template.md` | 所属目录 | `PROTOCOL_TEMPLATE.md` |
| 交接记录 | `*-handoff.md` | `process-model/handoffs/` | 暂无 |

### 2.3 文档状态矩阵

| 状态 | 标签 | 存放位置 | 处理方式 |
|------|------|----------|----------|
| 当前有效 | （无标签） | 正式区 | 默认优先读取 |
| 已冻结 | 目录 README 声明 | 正式区子目录 | 仍为正式口径，但不活跃更新 |
| 已归档 | `_archive/` 下 | 归档区 | 不承担当前真值 |
| 已过期 | N/A | 应删除 | 无追溯价值的中间产物 |

---

## 3. 文件命名规范

### 3.1 通用规则

```
<主题>-<描述>[-v<N>]．<ext>
```

- **语言**：同一目录内保持语言一致（推荐英文，因为工具链友好）
- **字符集**：仅使用 `a-z`、`0-9`、`-`（连字符），禁止空格、中文拼音、下划线
- **长度**：全路径不超过 120 字符，文件名不超过 60 字符
- **版本后缀**：`-v1`、`-v2`，禁止 `-V1`、`-v1.0`、`-v1.0.0`
- **日期**：不在文件名中嵌入日期（日期信息存放在 Git 历史中）

### 3.2 文档类型后缀

| 类型 | 后缀 | 必填 | 示例 |
|------|------|------|------|
| 基线/标准 | `-baseline`、`-standard` | 否 | `workspace-baseline.md` |
| 决策记录 | `ADR-NNN-` | 是 | `ADR-001-workspace-baseline.md` |
| 规范/契约 | `-contract` | 推荐 | `dxf-motion-execution-contract-v1.md` |
| 矩阵 | `-matrix` | 推荐 | `layered-test-matrix.md` |
| 清单 | `-checklist` | 推荐 | `release-test-checklist.md` |
| 模板 | `-template` | 是 | `protocol-template.md` |
| 报告 | `-report` | 推荐 | `system-acceptance-report.md` |
| 方案 | `-plan` | 推荐 | `legacy-cleanup-plan.md` |
| 复盘 | `-retro` | 推荐 | `sprint-retro.md` |

### 3.3 目录命名规范

- 使用小写 kebab-case（与 AGENTS.md 规则一致）
- 禁止使用中文目录名
- 禁止使用 `_` 前缀（除 `_archive/` 作为约定归档根目录）
- 禁止超过三级嵌套

### 3.4 现有文件重命名原则

- **不追溯修改已有文件**：避免破坏 Git 历史和外部引用链接
- **新文件严格执行**：从本方案生效之日起，所有新增文档遵守新规范
- **渐进式迁移**：仅在执行重大重构时顺带修正文件名

---

## 4. 目录结构设计（目标态）

### 4.1 顶层结构（不变）

```
docs/
├── README.md                     # 总入口
├── docs-organization-plan.md     # 本方案文档
│
├── architecture/                 # [正式] 架构文档
├── decisions/                    # [正式] 决策记录
├── machine-model/                # [正式] 机型事实
├── onboarding/                   # [正式] 开发者指引
├── runtime/                      # [正式] 运行手册
├── troubleshooting/              # [正式] 故障排查
├── validation/                   # [正式] 验证体系
│
├── process-model/                # [历史] 过程记录（精简版）
├── _archive/                     # [历史] 归档材料（重整版）
│
└── assets/                       # [新增] 共享资产
    └── images/                   #       全局共用图片
```

**变更说明**：
1. 删除 `architecture-audits/`（合并至 `architecture/history/audits/`）
2. 新增 `assets/images/` 集中存放全局共用图片
3. `process-model/` 保留目录但内部大幅精简
4. 顶层始终为 11 个目录 + 2 个文件

### 4.2 architecture/ 调整

**现状**：
```
architecture/
├── history/
│   ├── audits/          # 专项审计报告
│   └── ...
├── ...
```

**目标态**：保持不变，但将 `architecture-audits/` 合并至此目录下

```
architecture/
├── history/
│   ├── audits/          # 原 architecture/history/audits/ + architecture-audits/
│   ├── baselines/
│   ├── closeouts/
│   ├── module-boundary/
│   └── progress/
└── ...
```

### 4.3 process-model/ 精简方案

**现状**：
```
process-model/
├── plans/                # ~50 个文件（大量 NOISSUE-* 临时计划）
├── reviews/              # ~55 个文件（同一特性的多版本评审）
└── retro/                # 仅 README
```

**目标态**：
```
process-model/
├── README.md
├── plans/                # 仅保留 5-8 个代表性/汇总性计划文件
├── reviews/              # 仅保留 5-8 个最终版/汇总版评审文件
└── retro/                # 仅 README（保留空目录占位）
```

**清理策略**：
- **plans/**：只保留 wave 级的 closeout 汇总文件，删除所有单个 batch 的 scope/plan/test-plan
- **reviews/**：对同一 `feat-dispense-NOISSUE` 系列的多个时间戳版本，只保留最终版（最新时间戳）
- **批量移动**：被删除的文件统一移入 `_archive/process-model/`，不直接删除

### 4.4 _archive/ 结构调整

**现状**：
```
_archive/
├── README.md
└── 2026-03/
    ├── README.md
    ├── 整体重构方案（修订版）.md
    ├── 阶段0-基线盘点.md
    ├── 阶段0-迁移映射.md
    └── hmi-client/
        ├── README.md, CHANGELOG.md, etc.
        └── docs/
            ├── DEVELOPMENT_CONVENTIONS.md
            ├── TESTING_CONVENTIONS.md
            ├── backend-contract/PROTOCOL_TEMPLATE.md
            └── assets/imported-uploads/images/ (7 个图片文件)
```

**目标态**：
```
_archive/
├── README.md
├── process-model/              # 来自 process-model 精简移出的文件
│   ├── plans/
│   └── reviews/
├── hmi-client/                 # 扁平化：移除 docs/ 内层嵌套
│   ├── README.md
│   ├── CHANGELOG.md
│   ├── COMMIT_CONVENTIONS.md
│   ├── CONTRIBUTING.md
│   ├── PROJECT_BOUNDARY_RULES.md
│   ├── legacy-README.md
│   ├── development-conventions.md
│   ├── testing-conventions.md
│   └── protocol-template.md
└── 重构方案/                   # 中文名文件保持不动（避免 Git 混乱）
    ├── 整体重构方案（修订版）.md
    ├── 阶段0-基线盘点.md
    └── 阶段0-迁移映射.md
```

### 4.5 assets/ 新增

```
assets/
├── README.md
└── images/               # 从 _archive/ 迁移过来的全局共用图片
```

---

## 5. 重复或过时文档处理策略

### 5.1 判定标准

| 判定结果 | 条件 | 处理动作 |
|----------|------|----------|
| 确定过期 | 内容已被正式区文档取代，且无追溯价值 | 删除 |
| 可能过期 | 内容过时但可能有追溯价值 | 移入 `_archive/` |
| 完全重复 | 多份文档内容完全相同 | 只保留一份，其余删除 |
| 版本重复 | 同一主题有多个时间戳版本 | 只保留最终版/最新版，其余归档 |
| 部分重复 | 多份文档有重叠但各有独特内容 | 合并后归档旧版 |

### 5.2 具体处理清单

#### process-model/plans/ — 保留清单（其余移入 `_archive/process-model/plans/`）

保留理由：代表完整的 wave 收口证据链

- `NOISSUE-wave2b-closeout-20260322-191927.md`（wave2b 汇总收口）
- `NOISSUE-wave3a-closeout-20260322-194649.md`（wave3a 收口）
- `NOISSUE-wave3b-closeout-20260322-200303.md`（wave3b 收口）
- `NOISSUE-wave3c-closeout-20260322-202853.md`（wave3c 收口）
- `NOISSUE-wave4a-closeout-20260322-205132.md`（wave4a 收口）
- `NOISSUE-wave4b-closeout-20260322-210606.md`（wave4b 收口）
- `NOISSUE-wave4c-closeout-20260322-212031.md`（wave4c 收口）
- `NOISSUE-wave4d-closeout-20260322-212031.md`（wave4d 收口）
- `NOISSUE-wave4e-closeout-20260322-233503.md`（wave4e 收口）
- `NOISSUE-wave5c-merge-closeout-20260323-012030.md`（wave5c 收口）

#### process-model/reviews/ — 保留清单（其余移入 `_archive/process-model/reviews/`）

保留最终版/汇总版：
- `modules-review.md`（模块评审汇总）
- `modules-owner-boundary-review-revised-2026-03-27.md`（修订版）
- `modules-review-confidence-audit.md`（置信度审计）
- `architecture-review-recheck-report-20260331-082102.md`（架构复审报告）
- `refactor-arch-NOISSUE-architecture-refactor-spec-release-20260324-213217.md`（重构发布版）
- `feat-hmi-NOISSUE-dispense-trajectory-preview-v2-release-20260322-015424.md`（HMI 发布版）
- `feat-dispense-NOISSUE-e2e-flow-spec-alignment-release-20260323-012030-wave5c-merge.md`（最终合并版）
- `feat-dispense-NOISSUE-wave6c-local-validation-gate-release-20260323-112545.md`（最终验证版）
- `hmi-application-module-architecture-review-20260331-074433.md`（模块架构评审）
- `coordinate-alignment-module-architecture-review-20260331-074844.md`
- `dispense-packaging-module-architecture-review-20260331-074840.md`
- `motion-planning-module-architecture-review-20260331-075136-944.md`
- `process-path-module-architecture-review-20260331-074844.md`
- `process-planning-module-architecture-review-20260331-075201.md`
- `runtime-execution-module-architecture-review-20260331-075228.md`
- `topology-feature-module-architecture-review-20260331-075200.md`
- `refactor-workflow-ARCH-202-preview-compat-drain-workflow-module-architecture-review-20260331-074657.md`

#### decisions/history/legacy-cleanup/

- 保留 `legacy-cleanup-plan-v2.md`（v2 为最终版）
- 删除 `legacy-cleanup-plan.md`（被 v2 取代）
- 保留 `legacy-removal-execution.md`（执行记录有独立价值）

---

## 6. 文档版本控制方法

### 6.1 版本策略

| 机制 | 适用场景 | 说明 |
|------|----------|------|
| Git 历史 | 所有文档 | 默认版本控制工具，`git log` / `git blame` 追溯 |
| 文件名版本后缀 | 正式规范/冻结文档 | 如 `*-v1.md`、`*-v2.md`，只有发布新版本时才变更 |
| ADR 编号 | 架构决策 | 如 `ADR-001-*`，编号不重复使用 |
| README 版本声明 | 目录级别 | 在目录 README 中声明"当前生效版本" |

### 6.2 版本号约定

- **格式**：`v<主版本>`（如 `v1`、`v2`）
- **何时递增**：内容发生 breaking change 或正式发布新版本时
- **不递增的情况**：修正拼写、补充说明、重构格式而不改变语义
- **版本对应**：文件名中的 `-v1` 应和 Git tag（若有）对应

### 6.3 冻结文档管理

- 冻结文档放在正式区子目录中（如 `architecture/dsp-e2e-spec/`）
- 目录 README 显式声明为冻结
- 冻结文档仅在重大版本迭代时更新，平时只允许勘误
- 新版本应创建新文件（如 `*-v2.md`），旧文件保留不删除

---

## 7. 整理后维护机制

### 7.1 新增文档准入检查

每次向 `docs/` 新增文档时，必须通过以下检查：

```
□ 确定文档类型（正式/历史）
□ 确定存放目录（根据分类标准）
□ 文件名是否符合命名规范
□ 是否需要在目标目录 README 中补充入口链接
□ 是否已有同主题文档（避免重复）
□ 若包含正式结论，确保放入正式区而非历史区
```

### 7.2 定期审查机制

| 频率 | 审查内容 | 执行者 |
|------|----------|--------|
| 每次 PR | 新增文档是否符合命名规范和目录职责 | PR Reviewer |
| 每周 | process-model/ 是否有新的过多临时文件 | Codex Agent |
| 每月 | 正式区是否有内容过时需要归档的文档 | 维护者 |
| 每季度 | 全面审查 docs/ 目录健康度 | 架构组 |

### 7.3 process-model/ 容量警戒线

- **警戒阈值**：`process-model/` 总文件数超过 50
- **触发动作**：由 Codex Agent 在执行任务时主动提示清理
- **清理周期**：每次 wave/迭代结束后，归档该迭代的过程文件

### 7.4 外部引用保护

- 外部代码文件对 `docs/` 的引用应使用**相对路径**
- 调整文档位置时，必须检查并更新所有外部引用
- 引用错误应视为 **bug** 处理
- 当前已知外部引用清单：

| 引用源 | 引用目标 | 风险 |
|--------|----------|------|
| `docs/architecture/README.md` | `topics/` | 本目录内引用，安全 |
| `modules/hmi-application/README.md` | `docs/architecture-audits/...` | 计划变更，需更新 |
| `apps/hmi-app/COMPATIBILITY.md` | `docs/_archive/2026-03/hmi-client/` | 目标路径将变更，需更新 |
| `tests/e2e/hardware-in-loop/README.md` | `docs/validation/online-test-matrix-v1.md` | 目标不移，安全 |
| `modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md` | `docs/architecture/...` | 目标不移，安全 |
| `apps/trace-viewer/README.md` | `docs/validation/` | 目标不移，安全 |
| `modules/workflow/docs/session-handoffs/...` | `docs/architecture-audits/` | 计划变更，需更新 |

### 7.5 README 维护规则

- **每个子目录必须有 README.md**
- README 必须包含：目录职责、消费顺序、使用规则
- 正式区目录 README 必须声明当前生效版本和冻结状态
- 历史区目录 README 必须声明"不承担当前真值"
- README 在目录结构调整后必须同步更新

---

## 8. 执行计划

### 8.1 批次划分

| 批次 | 内容 | 工作量 | 风险 |
|------|------|--------|------|
| 批次 1 | `process-model/plans/` 精简归档 | 中 | 低 |
| 批次 2 | `process-model/reviews/` 精简归档 | 中 | 低 |
| 批次 3 | `architecture-audits/` → `architecture/history/audits/` 合并 | 低 | 中（需更新引用） |
| 批次 4 | `_archive/` 结构调整 | 中 | 中（需更新引用） |
| 批次 5 | `decisions/history/legacy-cleanup/` 清理 | 低 | 低 |
| 批次 6 | 更新外部引用路径 + README 统一 | 中 | 高（需验证） |
| 批次 7 | 最终验证 | 低 | 低 |

### 8.2 执行前置条件

- [ ] 确认所有仓库修改已提交（避免冲突）
- [ ] 确认当前工作区干净（git status）
- [ ] 批次 6 前确认所有外部引用路径已更新
- [ ] 批次 7 前确认无断裂引用

### 8.3 回滚策略

- 每个批次独立执行，不跨批次 commit
- 若某批次导致引用断裂，立即 revert 该批次 commit
- 图片迁移前先复制，再删除源文件
