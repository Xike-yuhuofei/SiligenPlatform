# Module Owner Boundary Contract

## 1. 目的

冻结 `ARCH-203` 在范围模块的 owner 边界，明确每个模块必须拥有的事实、必须退出的越权实现，以及后续 evidence 的最低要求。

## 2. 模块边界矩阵

| Module | Canonical Owner Focus | Must Stop Owning | Required Consumer Seam | Minimum Evidence |
| --- | --- | --- | --- | --- |
| `M0 workflow` | orchestration、phase progression、受控 compatibility shell | `M3-M11` live owner facts、长期 public helper target、`siligen_process_runtime_core` / `siligen_domain` 作为默认 build spine | 只允许通过 orchestration contract / facade 消费下游 | seam 列表、forbidden include 清单、build spine 退场证据、边界脚本 |
| `M3 topology-feature` | topology/feature owner objects 与稳定 facade | concrete adapter shadow copy、外部入口再导出 concrete adapter | 仅暴露声明的 contracts/facade | owner charter、shadow copy 处置、调用方切换证据 |
| `M4 process-planning` | `ProcessPlan` 及其 planning/config owner 语义 | app 层配置 owner、混装 `M1/M2/M9` 职责、被 `job-ingest/dxf-geometry` 逆向 concrete 依赖 | 通过 planning contracts 与下游交接 | owner map、app 外置实现回收计划、逆向依赖切断证据 |
| `M5 coordinate-alignment` | `CoordinateTransformSet`、`AlignmentCompensation`、对齐命令/结果 | machine/runtime 兼容子域、与 `workflow` 双 owner 副本 | 通过 alignment contracts 向 `M6` 交接 | owner artifact 列表、重复实现处置 |
| `M6 process-path` | `ProcessPath` 与 path application surface | `workflow` trajectory 副本、无边界输入适配、`workflow/dxf-geometry` 外部重复 path source adapter | 只暴露 canonical contracts/application facade | facade 使用面、旧路径收口证据、输入适配 owner 收口证据 |
| `M7 motion-planning` | `MotionPlan` 与规划求解事实 | 执行插补控制端口、runtime/control owner | 只暴露 planning contracts/facade | contracts 收口、执行端口迁移证据 |
| `M8 dispense-packaging` | trigger/CMP/dispense packaging owner facts | 点胶全域 service 集合、对 `workflow/runtime` 反向耦合、未编译 stub/source 与测试仍留在 `workflow/tests` | 通过 packaging contracts 向 `M9` 交接 | packaging owner map、M8/M0/M9 seam 证据、测试归属与死代码处置证据 |
| `M9 runtime-execution` | `ExecutionSession`、执行流程、执行状态、执行 contracts | 对 `workflow` 的长期 canonical 依赖、重复 device contract tree | 通过 execution application/runtime contracts 对宿主暴露 | M9 owner 收口、device/runtime contract 去重 |
| `M11 hmi-application` | HMI 前置阻断、会话状态、owner 决策对象 | 宿主 concrete client 反向依赖、宿主保留 owner 级规则、app 侧 compat re-export / owner 单测 / 无关链接依赖 | 宿主只通过模块 use case / contract 调用 | app/module 分层证据、宿主入口下沉计划、compat 测试与 link 收口证据 |

## 3. 全局禁止规则

1. 不允许把 `workflow` 内部路径、helper target 或 legacy include 当作长期稳定边界。
2. 不允许宿主应用或 gateway 直接拥有核心运行时事实对象。
3. 不允许新的影子实现进入 `workflow`、宿主应用或 duplicate contract tree。
4. 不允许在 compatibility shell 中新增业务逻辑。
5. 不允许把旧 build spine、旧 helper target、app 侧 compat tests/re-export 当作“实现已完成”的证据。

## 4. 完成判定

单个模块只有同时满足以下条件，才可被标记为 `boundary-closed`:

1. `Canonical Owner Focus` 已有明确代码落位。
2. `Must Stop Owning` 项已全部进入 `临时 bridge`、`只读兼容壳` 或 `移除目标`。
3. `Required Consumer Seam` 已被至少一个真实消费者使用。
4. `Minimum Evidence` 已存在且可复查。
