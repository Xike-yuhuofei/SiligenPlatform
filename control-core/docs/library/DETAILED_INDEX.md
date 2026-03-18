---
Owner: @Xike
Status: active
Last reviewed: 2026-03-15
Scope: 本项目全局
---

# 博派知识库详细索引（Library Index）

这是 `docs/library/` 的详细导航版本。想快速进入，请看 [00_index.md](./00_index.md)。

## 按角色查找

| 角色 | 建议入口 |
|------|----------|
| 新同学 | [04_guides/project-reproduction.md](./04_guides/project-reproduction.md) |
| 开发人员 | [04_guides/README.md](./04_guides/README.md) |
| 架构/技术负责人 | [02_architecture.md](./02_architecture.md) + [docs/adr/README.md](../adr/README.md) |
| 现场/运维 | [05_runbook.md](./05_runbook.md) + [06_reference.md](./06_reference.md) |

## 按任务查找

| 任务 | 推荐文档 |
|------|---------|
| 复现当前仓库 | [04_guides/project-reproduction.md](./04_guides/project-reproduction.md) |
| 理解整体架构 | [02_architecture.md](./02_architecture.md) |
| 查配置、IO、接口约束 | [06_reference.md](./06_reference.md) |
| 处理现场问题 | [05_runbook.md](./05_runbook.md) |
| 理解架构决策 | [docs/adr/README.md](../adr/README.md) |
| 查看知识库入口 | [00_index.md](./00_index.md) |

## 核心文档

### 00. 入口
- [00_index.md](./00_index.md)
- 适合快速分流，不适合做完整导航

### 01. 项目概览
- [01_overview.md](./01_overview.md)
- 适合先建立项目边界和当前目标

### 02. 系统架构
- [02_architecture.md](./02_architecture.md)
- 适合理解分层、依赖方向和模块职责

### 03. 架构决策
- [docs/adr/README.md](../adr/README.md)
- 当前以 `docs/adr/` 为唯一权威 ADR 目录；`library` 仅保留入口，不再维护 ADR 副本

### 04. 操作指南
- [04_guides/README.md](./04_guides/README.md)
- [04_guides/dispenser-purge.md](./04_guides/dispenser-purge.md)
- [04_guides/documentation-lifecycle.md](./04_guides/documentation-lifecycle.md)
- [04_guides/project-reproduction.md](./04_guides/project-reproduction.md)
- [04_guides/recipe-management.md](./04_guides/recipe-management.md)
- [04_guides/vcpkg-usage-guide.md](./04_guides/vcpkg-usage-guide.md)

### 05. 排障
- [05_runbook.md](./05_runbook.md)
- [../debug/](../debug/)

### 06. 参考
- [06_reference.md](./06_reference.md)
- [hardware-limit-configuration.md](./hardware-limit-configuration.md)
- [error-codes.md](./error-codes.md)
- [error-codes-list.md](./error-codes-list.md)

## 常见问题入口

| 问题 | 文档 |
|------|------|
| 控制卡连接失败 | [05_runbook.md](./05_runbook.md) |
| 电机无响应 | [05_runbook.md](./05_runbook.md) |
| 点胶阀/CMP 异常 | [05_runbook.md](./05_runbook.md) + [06_reference.md](./06_reference.md) |
| 配方如何操作 | [04_guides/recipe-management.md](./04_guides/recipe-management.md) |
| 当前仓库怎么构建 | [04_guides/project-reproduction.md](./04_guides/project-reproduction.md) |

## 相关文档区域

- 当前权威知识：[`docs/library/`](./)
- 架构决策：[`docs/adr/`](../adr/)
- 实现备忘：[`docs/implementation-notes/`](../implementation-notes/)
- 调试记录：[`docs/debug/`](../debug/)
- 计划草案：[`docs/plans/`](../plans/)
- 历史归档：[`docs/_archive/`](../_archive/)
