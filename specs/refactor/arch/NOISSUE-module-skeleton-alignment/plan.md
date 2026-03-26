# Plan: modules 统一骨架迁移蓝图

更新时间：`2026-03-25`

## 1. 总体策略

采用“统一骨架先落位、旧结构保留为迁移桥、按模块逐步收敛”的总体策略。

迁移期统一目标骨架如下：

```text
modules/<module>/
├─ README.md
├─ module.yaml
├─ contracts/
├─ domain/
├─ services/
├─ application/
├─ adapters/
├─ runtime/        # 仅运行态模块启用
├─ tests/
│  ├─ unit/
│  ├─ contract/
│  ├─ integration/
│  └─ regression/
├─ examples/
└─ src/            # 迁移期 bridge；终态退出
```

## 2. 模块分组

### 第一组：优先迁移的规划链模块

- `job-ingest`
- `dxf-geometry`
- `topology-feature`
- `process-planning`
- `coordinate-alignment`
- `process-path`
- `motion-planning`
- `dispense-packaging`

### 第二组：中高复杂度编排模块

- `workflow`

### 第三组：运行态与宿主耦合较强模块

- `runtime-execution`
- `trace-diagnostics`
- `hmi-application`

## 3. 迁移原则

- 先建骨架，再迁实现，再切构建，再删 bridge。
- `contracts/` 先稳定，跨模块依赖优先改到契约层。
- `domain/` 只放 owner 事实、值对象、不变量和纯领域规则。
- `services/` 只放模块用例编排。
- `application/` 负责 facade、handler、query/command 入口。
- `adapters/` 负责外部依赖接入。
- `runtime/` 只允许运行态模块使用。
- 迁移期 `src/` 不允许继续承载新增终态代码。

## 4. 阶段设计

1. 为全部一级模块补齐统一骨架顶层目录和 `module.yaml`。
2. 规划链模块逐个将 `src/` 中的实现迁入新骨架。
3. 将与模块直接相关的测试和样例归位到模块本地 `tests/` 与 `examples/`。
4. 逐模块把构建图从旧 `src/` 切换到新骨架。
5. 处理 `workflow`、`runtime-execution`、`trace-diagnostics`、`hmi-application` 等高复杂模块的职责净化与结构收敛。
6. 逐模块退出 `src/` bridge。

## 5. 完成判定

### 5.1 Final hard closeout

一个模块只有同时满足以下条件，才算完成统一骨架对齐的终态关闭：

- 顶层统一骨架已完整存在。
- 新增代码不再进入 `src/`。
- owner 实现已迁入 `domain/services/application/adapters/runtime` 对应层。
- 模块测试与样例已有正式承载面。
- 构建入口已不再依赖旧 `src/` 作为主实现来源。
- `module.yaml` 已声明 owner、依赖、禁止依赖和测试目标。
- `src/` 已删除，或仅剩说明性 tombstone，不再承载实现事实。

### 5.2 Phase 7 bridge-backed closeout

本轮实际执行采用的是 `bridge-backed closeout` 口径。满足以下条件即可判定 `Phase 7` 完成：

- 顶层统一骨架与 `contracts/`、`tests/*`、`examples/` 已在全部模块落位。
- 模块级 `CMakeLists.txt` 已声明 `SILIGEN_TARGET_IMPLEMENTATION_ROOTS` 与 `SILIGEN_BRIDGE_ROOTS`。
- 历史 bridge 目录已被显式标记为 `legacy-source bridge`。
- 主构建元数据已把统一骨架标识为默认实现来源，bridge 仅承担兼容或回退职责。
- 模块 README、清单与波次映射已记录 closeout 证据。

说明：

- `Phase 7 bridge-backed closeout` 不等价于 bridge 目录物理清空。
- `workflow/process-runtime-core`、`runtime-execution/runtime-host`、`runtime-execution/device-adapters`、`runtime-execution/device-contracts` 当前仍保留兼容实现与 fallback 入口。
- 若后续要达成 `Final hard closeout`，需要在新阶段继续清空兼容桥并做真实 cutover。
