# 架构决策记录 (Architecture Decision Records)

本目录包含当前 Siligen Backend C++ 项目的重要架构决策记录（ADR）。

---

## 📋 什么是 ADR？

ADR (Architecture Decision Record) 是记录重要架构决策的文档，用于：
- ✅ 保存决策的**上下文**和**理由**
- ✅ 记录决策的**后果**（正面和负面）
- ✅ 提供**可追溯性**（为什么我们这样做？）
- ✅ 帮助新人理解架构演变历史

---

## 🎯 ADR 生命周期

每个ADR经历以下状态：

```
提议 (proposed) → 已接受 (accepted) → 已弃用 (deprecated) → 已替代 (superseded)
```

| 状态 | 说明 | 示例 |
|------|------|------|
| `proposed` | 决策已提出，待讨论 | ADR-005: 采用C++20模块 |
| `accepted` | 决策已通过，正在实施 | ADR-001: 采用六边形架构 |
| `deprecated` | 决策已过时，但仍有效 | ADR-003: 使用Result<T> (已考虑使用std::expected) |
| `superseded` | 决策已被新ADR替代 | ADR-XXX: 被ADR-YYY替代 |

---

## 📚 ADR 索引

| 编号 | 标题 | 状态 | 日期 | 影响 |
|------|------|------|------|------|
| [ADR-0001](./ADR-0001-hexagonal-architecture.md) | 采用六边形架构 | Accepted | 2025-12-01 | 架构设计 |
| [ADR-0002](./ADR-0002-namespace-unification.md) | 统一命名空间 | Accepted | 2025-12-05 | 代码组织 |
| [ADR-0003](./ADR-0003-result-pattern.md) | 使用Result<T>模式 | Accepted | 2025-12-10 | 错误处理 |
| [ADR-0005](./ADR-0005-domain-services-source-of-truth.md) | 领域服务成为业务规则唯一入口 | Accepted | 2026-02-04 | 领域规则 |
| [ADR-0006](./ADR-0006-interlock-policy-integration.md) | 互锁策略统一与安全检查集成 | Accepted | 2026-02-04 | 安全策略 |
| [ADR-0007](./ADR-0007-trajectory-geometry-utils.md) | 轨迹几何工具抽取与Boost适配 | Accepted | 2026-02-04 | 轨迹几何 |
| [ADR-0008](./ADR-0008-infrastructure-adapter-alignment.md) | 基础设施适配器对齐领域服务 | Accepted | 2026-02-04 | 适配器集成 |
| [ADR-0009](./ADR-0009-domain-service-unit-tests.md) | 领域服务单元测试基线 | Accepted | 2026-02-04 | 测试策略 |
| [ADR-0010](./ADR-0010-remove-legacy-sim-driver.md) | 移除遗留仿真驱动与过时文档 | Accepted | 2026-02-04 | 维护清理 |
| [ADR-0011](./ADR-0011-adr-canonical-location.md) | ADR 权威目录统一到 docs/adr | Accepted | 2026-03-17 | 文档治理 |

---

## ✍️ 如何创建新ADR

### 🎖️ 推荐方法: 使用 @lordcraymen/adr-toolkit (adrx)

从 2026-01-11 开始，项目使用 **adrx** 工具管理 ADR：

```bash
# 1. 安装 adrx（首次使用）
npm install -g @lordcraymen/adr-toolkit

# 2. 创建新 ADR（交互式）
adrx create

# 3. 验证格式
adrx check docs/adr/ADR-*.md

# 4. 生成索引
adrx build
```

**adrx 优势**：
- ✅ 自动验证 YAML frontmatter 格式
- ✅ 自动生成 ADR 索引
- ✅ GitHub PR 集成
- ✅ AI 友好（支持离线操作）
- ✅ 依赖精简（仅 3 个依赖）

### 📝 ADR 格式说明

#### 新 ADR 格式（adrx 标准，2026-01-11 起）

必需使用标准 YAML frontmatter 格式：

```markdown
---
title: "ADR-0005: Meaningful title"
date: "2026-01-11"
status: Proposed
tags:
  - architecture
modules:
  - src/
summary: >-
  Short one-paragraph summary (max 300 characters) describing the decision and why it matters.
---

# Context

Describe the forces at play, relevant background and constraints.

# Decision

State the decision that was made.

# Consequences

Explain the positive and negative outcomes from this decision.

# References

- Link to related ADRs or documentation.
```

**必需字段**：
- `title` - ADR 标题（格式：`ADR-XXXX: 描述`）
- `status` - 状态（accepted/proposed/rejected/superseded/deprecated）
- `summary` - 一段总结（最多 300 字符）

**可选字段**：
- `date` - 决策日期
- `tags` - 标签数组
- `modules` - 受影响的模块路径

### 📋 手动创建（不推荐）

如果 adrx 不可用，可以手动创建：

```bash
# 1. 复制模板
cp docs/adr/templates/ADR-0000-template.md docs/adr/ADR-0005-your-decision.md

# 2. 编辑ADR，填写所有字段
vim docs/adr/ADR-0005-your-decision.md

# 3. 验证格式
adrx check docs/adr/ADR-0005-your-decision.md

# 4. 创建PR，请求讨论
gh pr create --title "ADR-0005: Your Decision Title"
```

---

## 📝 ADR 模板

创建新ADR时，建议使用 **adrx 工具**自动生成模板（参见上文"如何创建新ADR"）。

如需手动创建，参考 [ADR模板](./templates/ADR-0000-template.md)，必须包含以下字段：

### 必需字段

- **编号**: ADR-XXX（递增编号）
- **标题**: 简洁描述决策
- **状态**: proposed/accepted/deprecated/superseded
- **日期**: YYYY-MM-DD
- **作者**: 决策提出者
- **上下文**: 为什么要做这个决策？
- **决策**: 具体决定了什么？
- **后果**: 正面和负面影响

### 可选字段

- **替代方案**: 考虑过的其他方案
- **相关决策**: 相关的ADR链接
- **实施状态**: 实施进度（0%, 50%, 100%）
- **审查日期**: 下次审查日期

---

## 🔍 ADR 审查流程

### 季度审查（每3个月）

- [ ] 检查所有 `accepted` 状态的ADR是否仍然有效
- [ ] 更新实施状态
- [ ] 将过时的ADR标记为 `deprecated` 或 `superseded`
- [ ] 记录审查结果到 [REVIEW-LOG](../library/REVIEW-LOG.md)

### 审查清单

| ADR编号 | 标题 | 是否仍有效？ | 需要更新？ | 行动 |
|---------|------|-------------|-----------|------|
| ADR-001 | 采用六边形架构 | ✅ 是 | ❌ 否 | 无 |
| ADR-003 | Result<T>模式 | ⚠️ 部分 | ✅ 是 | 考虑C++23 std::expected |

---

## 🔗 相关资源

- [ADR模板](./templates/ADR-0000-template.md)
- [ADR审查日志](../library/REVIEW-LOG.md)
- [ADR最佳实践](https://adr.github.io/)
- [项目架构文档](../library/02_architecture.md)

---

## 📊 统计信息

- **总ADR数量**: 9
- **活跃ADR**: 9（全部 accepted）
- **已弃用ADR**: 0
- **已替代ADR**: 0
- **待处理提议**: 0

---

## 📧 联系方式

如有关于ADR的疑问或建议，请联系：

- **架构师**: @lead-architect
- **技术委员会**: @technical-committee

---

**最后更新**: 2026-02-04
**维护者**: @Xike
**ADR 工具**: @lordcraymen/adr-toolkit (adrx) v3.4.0
