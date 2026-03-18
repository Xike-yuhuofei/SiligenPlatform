# Siligen 文档总览

这里汇总当前仓库仍在维护的文档入口。`docs/library/` 是面向使用者和开发者的权威说明，`docs/plans/` 与 `docs/_archive/` 保留历史过程，不应直接视为当前实现说明。

## 从哪里开始

### 新接手仓库

1. [当前项目复现指南](./library/04_guides/project-reproduction.md)
2. [项目概览](./library/01_overview.md)
3. [系统架构](./library/02_architecture.md)
4. [参考手册](./library/06_reference.md)

### 日常开发

1. [知识库入口](./library/00_index.md)
2. [Guides 索引](./library/04_guides/README.md)
3. [ADR 索引](./adr/README.md)
4. [配方管理指南（TCP/HMI）](./06-user-guides/recipe-management.md)

### 故障排查

1. [Runbook 排障手册](./library/05_runbook.md)
2. [调试记录](./debug/)
3. [实现笔记](./implementation-notes/)

## 当前文档结构

```text
docs/
  library/               当前权威文档与索引
  adr/                   架构决策记录
  04-development/        专题开发说明
  06-user-guides/        面向使用者的操作说明
  implementation-notes/  实现层备忘与可行性记录
  debug/                 问题分析与排障记录
  plans/                 计划与设计草案
  _archive/              历史归档
```

## 维护约定

- 当前实现、当前入口、当前命令，优先写入 `docs/library/`。
- 仍在讨论或未落地的方案，放入 `docs/plans/`。
- 已完成但不再作为权威依据的过程文档，移入 `docs/_archive/`。
- 索引文档只保留当前仓库能打开的路径，不再引用已删除目录。
