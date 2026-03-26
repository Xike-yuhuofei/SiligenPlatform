# Wave Mapping: modules 统一骨架对齐

更新时间：`2026-03-25`

## Purpose

本文件定义模块统一骨架对齐的迁移波次、模块分组和每波的进入/退出条件。

## Wave Summary

| Wave | Scope | Modules | Main objective |
|---|---|---|---|
| `Wave 0` | 统一骨架顶层落位 | `M0-M11` | 所有模块先具备同一顶层骨架与 bridge 语义 |
| `Wave 1` | 上游与规划链模块实现迁移 | `M1-M8` | 建立第一批真实对齐样板 |
| `Wave 2` | `workflow` 职责净化 | `M0` | 将历史聚合目录降级为 bridge |
| `Wave 3` | 运行态与宿主耦合模块收敛 | `M9-M11` | 完成高复杂模块统一骨架对齐 |
| `Wave 4` | 全模块构建切换与 bridge 退出 | `M0-M11` | 停止以历史 `src/` 为主实现来源 |

## Wave Conditions

| Wave | Entry gate | Exit gate |
|---|---|---|
| `Wave 0` | 统一骨架目录、bridge 规则和模块分组已确认 | `module.yaml`、`domain/`、`services/`、`application/`、`adapters/`、`tests/`、`examples/` 已在所有模块落位 |
| `Wave 1` | `Wave 0` 已完成 | `M1-M8` 的 owner 实现已迁出 `src/` 主实现路径 |
| `Wave 2` | `Wave 1` 已完成 | `workflow` 的历史聚合目录仅剩 bridge 角色 |
| `Wave 3` | `Wave 2` 已完成 | `M9-M11` 已完成统一骨架对齐并保留必要受控特化 |
| `Wave 4` | `Wave 3` 已完成 | 全部模块完成 build switch 与 bridge downgrade |

## Module Placement

| Module | Wave | Notes |
|---|---|---|
| `workflow` | `Wave 2` | 高复杂度聚合模块，后置 |
| `job-ingest` | `Wave 1` | 低复杂度优先样板 |
| `dxf-geometry` | `Wave 1` | 需处理 `engineering-data/` 相关资产 |
| `topology-feature` | `Wave 1` | 与上游几何边界协同收敛 |
| `process-planning` | `Wave 1` | 规划规则从宽泛 `src/` 拆出 |
| `coordinate-alignment` | `Wave 1` | 与运行态边界要提前切清 |
| `process-path` | `Wave 1` | 路径 owner 应先稳定 |
| `motion-planning` | `Wave 1` | 需先做 `M7/M9` 边界复核 |
| `dispense-packaging` | `Wave 1` | 与 `M7/M9` 邻接边界敏感 |
| `runtime-execution` | `Wave 3` | 允许 `runtime/` 受控特化 |
| `trace-diagnostics` | `Wave 3` | 不得反向改写上游事实 |
| `hmi-application` | `Wave 3` | 需保持与宿主壳分离 |

## Status

- Wave 0-Wave 4 已按 bridge-backed 统一骨架策略完成。
- 当前模块目录、`module.yaml`、bridge 标记与模块级 build-switch 元数据已经收口。
- `Phase 8` 已完成 `runtime-execution` canonical-first build cutover，并将 `runtime-host/tests`、`device-adapters/vendor/multicard` 锁定为 strict shell bridge roots。
- `Phase 9` 已完成 `workflow` canonical public surface owner 化，并验证 `runtime-host`、`planner-cli`、`transport-gateway` 可通过该 public surface 编译。
- `Phase 10` 已完成 `runtime-execution` 三个 bridge 根的 shell-only closeout；`device-contracts`、`device-adapters`、`runtime-host` 已不再承载真实 payload。
- `Phase 11` 已完成 `workflow` 两个 bridge 根的 shell-only closeout；`src` 与 `process-runtime-core` 已不再承载真实 payload。
- `Phase 12` 已完成 `trace-diagnostics`、`topology-feature`、`coordinate-alignment` 的 shell-only closeout，并为 `process-planning` 落地 closeout-ready 前置条件。
- `Phase 13` 已完成 `process-planning`、`process-path`、`dispense-packaging` 的 shell-only closeout，并把 `workflow` planning/dispensing interface target 切到新的模块 canonical target 转发。
- `Phase 14` 已完成 `motion-planning` canonical owner target 激活，并加固 `workflow` canonical tests 的 supporting target / 测试登记链。
- 当前状态表示：`runtime-execution`、`workflow`、`trace-diagnostics`、`topology-feature`、`coordinate-alignment`、`process-planning`、`process-path`、`dispense-packaging` 均已达到 `canonical-only implementation; shell-only bridges`；`motion-planning` 当前为 `bridge-backed; canonical owner target active; residual runtime/control surface frozen`；其余模块仍按既定 bridge-backed closeout 管理。
- 如需达到系统级无兼容桥残留的硬切换终态，仍需在后续新波次中继续执行剩余模块的 bridge drain 与真实 cutover。

