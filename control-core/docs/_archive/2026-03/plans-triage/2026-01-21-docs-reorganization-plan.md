---
Created: 2026-01-21
Status: planning
Owner: @Xike
---

# docs 目录整理方案

## 1. 现状分析

### 1.1 当前目录结构

```
docs/
├── _archive/           # 176 个 .md 文件（按 YYYY-MM 归档）
├── adr/                # 3 个 ADR + 模板
├── ADR_GUIDELINES.md   # 根目录散落文件
├── analysis/           # 2 个分析文档（不在六层体系中）
├── architecture/       # 1 个架构文档（不在六层体系中）
├── CLAUDE.md           # 文档规范
├── debug/              # 2 个调试记录
├── development/        # 1 个开发指南（不在六层体系中）
├── implementation-notes/ # 2 个实现笔记
├── library/            # 核心知识库（12 个文件）
├── plans/              # 49 个计划文档
├── quick-reference/    # 5 个快速参考（不在六层体系中）
├── README.md           # 文档入口
└── verification/       # 验证报告（不在六层体系中）
```

### 1.2 docs/CLAUDE.md 定义的六层体系

| 层级 | 目录 | 用途 |
|------|------|------|
| TIER_1 | library/ | Single Source of Truth，稳定知识 |
| TIER_2 | _archive/ | 历史记录，按 YYYY-MM 归档 |
| TIER_3 | implementation-notes/ | 工作笔记、技术备忘 |
| TIER_4 | debug/ | 故障排查记录 |
| TIER_5 | plans/ | 实施计划、设计方案 |
| TIER_6 | adr/ | 架构决策记录 |

### 1.3 发现的问题

#### P1: 目录不符合六层体系

以下目录不在 docs/CLAUDE.md 定义的六层体系中：

| 目录 | 文件数 | 建议处理 |
|------|--------|----------|
| analysis/ | 2 | 合并到 library/ 或 _archive/ |
| architecture/ | 1 | 合并到 library/ |
| development/ | 1 | 合并到 library/04_guides/ |
| quick-reference/ | 5 | 合并到 library/ |
| verification/ | 2+ | 合并到 _archive/ 或 library/ |

#### P2: ADR 重复和分散

ADR 相关文件分布在多个位置：

1. `docs/adr/` - 正式 ADR 目录（3 个 ADR）
2. `docs/library/03_decisions/` - library 内的 ADR 副本
3. `docs/ADR_GUIDELINES.md` - 根目录散落
4. `docs/library/ADR-QUICKSTART.md` - library 内的快速入门
5. `docs/plans/2026-01-11-adr-*.md` - 4 个 ADR 相关计划

问题：
- `ADR-0001-hexagonal-architecture.md` 在 adr/ 和 library/03_decisions/ 都有副本
- ADR 模板在两个位置：`adr/templates/` 和 `library/03_decisions/`

#### P3: library/ 索引引用缺失文件

`library/00_index.md` 引用了不存在的文件：
- `01_overview.md` - 不存在
- `00_toolchain.md` - 不存在

#### P4: 根目录散落文件

- `ADR_GUIDELINES.md` 应该移入 `adr/` 或 `library/`

#### P5: plans/ 目录膨胀

49 个计划文档，部分已完成但未归档：
- 多个 soft-limit 相关计划（重复/版本迭代）
- 多个 ADR 相关分析（已完成）

---

## 2. 整理方案

### 2.1 目标

1. 严格遵循 docs/CLAUDE.md 定义的六层体系
2. 消除重复文档
3. 修复断链
4. 清理已完成的计划

### 2.2 Phase 1: 消除非标准目录

#### 2.2.1 analysis/ -> library/ 或 _archive/

| 文件 | 处理 | 理由 |
|------|------|------|
| domain-layer-best-practices-2026.md | -> library/ | 稳定知识，长期价值 |
| usecase-test-architecture-issue.md | -> _archive/2026-01/ | 问题分析，时效性 |

#### 2.2.3 development/ -> library/04_guides/

| 文件 | 处理 | 理由 |
|------|------|------|
| vcpkg-usage-guide.md | -> library/04_guides/ | 开发指南 |

#### 2.2.4 quick-reference/ -> library/ 或 _archive/

| 文件 | 处理 | 理由 |
|------|------|------|
| error-codes.md | -> library/ | 规范文档 |
| error-codes-list.md | -> library/ | 参考文档 |
| test_limit_diagnosis_guide.md | -> library/ | 指南文档 |
| test_limit_diagnosis_summary.md | -> library/ | 参考文档 |

整理后删除空目录 `quick-reference/`。

#### 2.2.5 verification/ -> _archive/ 或 library/

| 文件 | 处理 | 理由 |
|------|------|------|
| 2026-01-14-limit-switch-verification-report.md | -> _archive/2026-01/ | 验证报告，时效性 |
| architecture/*.json | -> _archive/2026-01/ | 验证数据 |

### 2.3 Phase 2: 统一 ADR 管理

#### 2.3.1 确定 ADR 权威位置

根据 docs/CLAUDE.md 规定，ADR 应在 `adr/` 目录。

#### 2.3.2 整合 ADR 指南

| 操作 | 说明 |
|------|------|
| 移动 | `ADR_GUIDELINES.md` -> `adr/GUIDELINES.md` |
| 保留 | `library/ADR-QUICKSTART.md`（作为快速入门） |

**注意**: library/03_decisions/ 保持现状，不做处理。

### 2.4 Phase 3: 修复 library/ 索引

#### 2.4.1 创建缺失文件

| 文件 | 内容来源 |
|------|----------|
| library/01_overview.md | 从 README.md 提取系统概述部分 |
| library/00_toolchain.md | 新建，记录构建工具链 |

#### 2.4.2 更新索引

更新 `library/00_index.md` 和 `library/DETAILED_INDEX.md`，确保所有链接有效。

---

## 3. 执行顺序

### Step 1: 备份

```bash
# 创建备份
cp -r docs docs.backup.2026-01-21
```

### Step 2: 修复断链（优先级最高）

1. 创建 `library/01_overview.md`
2. 创建 `library/00_toolchain.md`

### Step 3: 消除非标准目录

按 Phase 2.2 执行，每个目录单独处理。

### Step 4: 统一 ADR

按 Phase 2.3 执行（仅移动 ADR_GUIDELINES.md）。

### Step 5: 更新索引

更新所有索引文件，确保链接有效。

### Step 6: 验证

```bash
# 检查断链
markdown-link-check docs/**/*.md
```

---

## 4. 风险和注意事项

### 4.1 风险

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 断链 | 文档无法访问 | 执行前备份，执行后验证 |
| 信息丢失 | 重要内容被误删 | 归档而非删除 |
| 索引不一致 | 导航混乱 | 统一更新所有索引 |

### 4.2 注意事项

1. **不删除，只归档**：所有"删除"操作实际上是移动到 `_archive/`
2. **保持原子性**：每个 Phase 独立执行，可以分批进行
3. **更新引用**：移动文件后，搜索并更新所有引用该文件的链接
4. **遵循规范**：所有新建/移动的 library 文档必须添加元数据

---

## 5. 预期结果

整理后的目录结构：

```
docs/
├── _archive/           # 历史归档（按 YYYY-MM）
│   └── 2026-01/
│       ├── analysis/   # 历史分析
│       └── ...
├── adr/                # 架构决策记录（权威位置）
│   ├── ADR-0001-hexagonal-architecture.md
│   ├── ADR-0002-namespace-unification.md
│   ├── ADR-0003-result-pattern.md
│   ├── GUIDELINES.md   # 从根目录移入
│   ├── README.md
│   └── templates/
├── debug/              # 调试记录
├── implementation-notes/ # 实现笔记
├── library/            # 核心知识库
│   ├── 00_index.md
│   ├── 00_toolchain.md # 新建
│   ├── 01_overview.md  # 新建
│   ├── 02_architecture.md
│   ├── 03_decisions/   # 保持现状
│   ├── 04_guides/
│   │   ├── README.md
│   │   └── vcpkg-usage-guide.md  # 从 development/ 移入
│   ├── 05_runbook.md
│   ├── 06_reference.md
│   ├── ADR-QUICKSTART.md
│   ├── DETAILED_INDEX.md
│   ├── domain-layer-best-practices.md  # 从 analysis/ 移入
│   ├── error-codes.md  # 从 quick-reference/ 移入
│   ├── error-codes-list.md
│   ├── hardware-limit-configuration.md
│   ├── REVIEW-LOG.md
│   ├── test-limit-diagnosis-guide.md  # 从 quick-reference/ 移入
│   └── test-limit-diagnosis-summary.md
├── plans/              # 保持现状
├── CLAUDE.md           # 文档规范
└── README.md           # 文档入口
```

删除的空目录：
- `development/` (vcpkg-usage-guide 已移入 library)
- `quick-reference/` (内容已分流到 library 和 _archive)

---

## 6. 待确认事项

在执行前需要确认：

1. [x] plans/ 处理方式 -> 已确认：保持现状，不做清理
2. [x] library/03_decisions/ 处理方式 -> 已确认：保持现状，不做处理
3. [ ] 是否需要更新 .claude/rules/ 中的相关规则？

---

## 7. 参考文档

- docs/CLAUDE.md - 文档规范
- .claude/rules/CONTEXT_MAP.md - 上下文加载规则
- .claude/rules/HEXAGONAL.md - 六边形架构规则
