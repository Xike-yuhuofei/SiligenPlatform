# 依赖规则

## 1. 目的

本文件定义当前工作区正式依赖方向。
它约束新增和重构中的 live 依赖，不再把已退出根或 package-era 依赖图写成 current canonical 事实。

## 2. 当前正式依赖层次

```text
apps/*
    -> modules/* public surface
    -> shared/*

modules/*
    -> own module internals
    -> foreign module public surface
    -> shared/*

shared/*
    -> shared/*
    -> support/vendor only when necessary
```

补充约束：

1. `apps/` 只负责进程入口、装配、发布壳与 app-local composition，不承接一级业务 owner。
2. `modules/` 是唯一正式业务 owner 面；跨模块协作必须停在 foreign module public surface 或 `shared/contracts/*`。
3. `shared/` 只承载稳定契约、kernel、messaging、logging、testing 等低业务含义公共层，不能反向吸入业务 owner。

## 3. 明确禁止的依赖

| 禁止依赖 | 原因 |
|---|---|
| `apps/* -> modules/*` 私有实现目录 | app 只能消费模块 public surface，不能越过边界直连 owner 内部实现 |
| `apps/hmi-app -> modules/runtime-execution` 设备适配或执行内部实现 | UI 不直接依赖硬件适配或执行实现 |
| `apps/* -> shared/*` 之外的历史根 | 已退出根不得重新进入默认依赖图 |
| 任意 live 代码 -> 已退役管理 surface | 已退役管理链已退出主线；DXF 固定参数 owner 已收敛到 runtime `production_baseline`，不得恢复旧 owner 链，也不得重新引入已删除的 serializer / CRUD surface |
| `modules/workflow -> retired-manager / runtime / engineering concrete` | workflow 只保留 orchestration / port 边界，不能重新聚合 foreign concrete |
| `modules/dxf-geometry -> motion-planning` trajectory concrete owner 回流 | trajectory owner 已迁到 `modules/motion-planning/` |
| `modules/* -> foreign module` 非公开头、私有源码目录或 app-local wiring | 跨 owner 依赖必须可审计、可替换 |
| `shared/contracts/* -> 任意业务实现` | 稳定契约必须保持中立 |
| `shared/kernel`、`shared/logging`、`shared/testing` -> 业务 owner 目录 | shared 只能向下稳定，不能被业务反向污染 |
| 任意 live 文件 -> `packages/`、`integration/`、`tools/`、`examples/` | 这些根已退出 current root set，不得被写回默认链路 |

## 4. 当前阶段执行口径

1. 文档、脚本、schema、测试基线与样本路径必须直接落在 `apps/`、`modules/`、`shared/`、`tests/`、`scripts/`、`config/`、`data/`、`samples/` 等 current canonical roots。
2. 若因兼容或审计需要提及历史根，必须显式标注为“历史 / 兼容 / 已退出”，不能写成当前 owner 或默认入口。
3. 新增跨模块能力时，先判断是 `shared/contracts/*` 稳定契约，还是某个 `modules/*` 的 owner public surface；禁止先落地实现再事后补 owner。
4. `apps/runtime-service` 持有 process bootstrap、workspace asset path 解析与 app-local wiring；这些能力不应再被泛化写成 `runtime-execution` 或旧 runtime-host 聚合依赖。
5. `samples/`、`shared/contracts/engineering/fixtures/` 与 `data/` 分别承载稳定样本、契约 fixture 与业务数据，不得混写为单一 baseline 根。

## 5. 依赖警戒线

以下情况出现时，应先停下补治理，而不是继续堆实现：

- 新功能需要同时改三个以上模块 owner，且没有明确 public surface。
- app 为了拿到功能，开始直接 include 模块私有头或拼接 owner 内部路径。
- `shared/*` 出现业务状态机、业务规则或单模块专属 DTO。
- 任何 live 文档、脚本、构建或源码重新把已退出根写回默认链路。
- 模块 public surface 与 README/边界文档描述长期不一致，导致 consumer 只能依赖私有实现。

## 6. 与 shared 边界的关系

当前 `shared/*` 的正式分工如下：

```text
shared/contracts
    <- 跨模块稳定契约

shared/kernel
    <- 低业务含义公共基础

shared/testing
    <- 仓库级测试支撑

shared/logging
    <- 稳定日志契约与公共日志基础
```

这些目录只定义稳定公共层，不代表任意历史实现已自动迁移完成。
若某个能力仍在收口中，应在 owner 模块边界页或 history 文档中说明，而不是把 shared 目标态误写成 current concrete。
