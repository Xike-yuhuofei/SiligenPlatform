---
title: "ADR-0011: ADR 权威目录统一到 docs/adr"
date: "2026-03-17"
status: Accepted
tags:
  - documentation
  - adr
  - governance
modules:
  - docs/adr
  - docs/library
  - docs/plans
summary: >-
  明确 docs/adr 为 ADR 的唯一权威目录，library 只保留入口链接与解读，不再承载 ADR 副本、模板或新的正式决策文档。
---

# Context

仓库曾同时在以下位置放置 ADR 或 ADR 模板：

- `docs/adr/`
- `docs/library/03_decisions/`
- `docs/plans/2026-01-11-adr-*.md`

这导致两个问题：

1. 新增 ADR 时，维护者不容易判断应写入哪个目录。
2. `docs/library/` 的权威知识与 `docs/adr/` 的正式决策边界被混淆，违反 Single Source of Truth。

当前 `docs/README.md`、`docs/library/DETAILED_INDEX.md` 与团队日常使用方式，已经事实性地把 `docs/adr/` 当作正式 ADR 入口，但仍残留少量旧副本与历史分析。

# Decision

1. `docs/adr/` 是本仓库 ADR 的唯一权威目录。
2. 新增、修改、审查正式 ADR 时，只使用 `docs/adr/`。
3. `docs/library/` 只保留 ADR 入口链接、阅读导航和面向使用者的解释性文档，不再新增 ADR 副本、模板或第二套正式 ADR 索引。
4. `docs/plans/` 中与 ADR 目录选型相关的分析文档完成使命后归档到 `docs/_archive/`。
5. `docs/library/03_decisions/` 视为历史过渡区域：在完成后续清理前不再新增内容，也不作为权威来源引用。

# Consequences

## 正面影响

- ADR 的写入位置和阅读入口更清晰。
- 文档分层更稳定：`library` 讲“当前事实”，`adr` 讲“为何如此”，`plans` 讲“尚未定案”。
- 降低重复维护两套 ADR 索引和模板的成本。

## 负面影响

- 需要更新现有索引，避免继续链接到旧副本目录。
- 过渡期内仍可能保留少量历史 ADR 副本，需在后续文档清理中继续收口。

# References

- [ADR 索引](./README.md)
- [文档总览](../README.md)
- [文档分层与收口规则](../library/04_guides/documentation-lifecycle.md)
- [归档：ADR 存放位置方案对比分析](../_archive/2026-03/plans-triage/2026-01-11-adr-unification-analysis.md)
