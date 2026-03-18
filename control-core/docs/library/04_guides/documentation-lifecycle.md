---
Owner: @Xike
Status: active
Last reviewed: 2026-03-17
Scope: docs/ 目录治理
---

# 文档分层与收口规则

本文定义 `docs/` 目录当前采用的文档分层与收口约定，用于取代旧的目录整理方案草稿。

## 1) 目录分工

| 目录 | 用途 | 是否权威 |
|---|---|---|
| `docs/library/` | 当前事实、当前入口、当前操作方式 | 是 |
| `docs/adr/` | 已接受或待审的正式架构决策 | 是 |
| `docs/plans/` | 未定案、未落地的方案与设计草稿 | 否 |
| `docs/debug/` | 现场问题排查记录 | 否 |
| `docs/implementation-notes/` | 实现备忘与可行性记录 | 否 |
| `docs/_archive/` | 已完成或不再作为主依据的过程文档 | 否 |

## 2) 收口原则

- 当前事实写进 `library`
- 架构决策写进 `adr`
- 仍在讨论的方案留在 `plans`
- 只具备历史价值的过程文档移入 `_archive`

一句话：`library` 解决“现在应该看什么”，`adr` 解释“为什么这样定”，`plans` 记录“还没决定什么”。

## 3) ADR 目录规则

- `docs/adr/` 是 ADR 的唯一权威目录。
- `docs/library/` 可以链接 ADR，但不再承载 ADR 副本、模板或第二套正式索引。
- 与 ADR 目录选型、工具选型有关的分析稿，完成使命后应归档。

详见：`docs/adr/ADR-0011-adr-canonical-location.md`

## 4) 计划文档如何退出主线

当计划满足以下任一条件时，应进行 triage：

1. 已被代码实现吸收
2. 已被 `library` 或 `adr` 收口
3. 已被 canonical 架构替代
4. 只剩历史分析价值

推荐动作：

- 提炼为 `library` 后归档原计划
- 提炼为 `ADR` 后归档原分析
- 直接归档
- 冻结并保留在 `plans/`，等待业务条件成熟

## 5) 本轮 triage 的落地结果

2026-03-17 已对一批 `docs/plans/` 历史方案做收口：

- ADR 目录选型分析 -> `docs/adr/ADR-0011-adr-canonical-location.md`
- 排胶方案 -> `docs/library/04_guides/dispenser-purge.md`
- 限位诊断结论 -> `docs/library/hardware-limit-configuration.md`
- 其余历史计划 -> `docs/_archive/2026-03/plans-triage/`

## 6) 参考

- `docs/README.md`
- `docs/_archive/README.md`
- `docs/adr/README.md`
