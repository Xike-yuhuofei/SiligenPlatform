# Module Skeleton Alignment Checklist

更新时间：`2026-03-25`

## Purpose

本文件定义 `modules/` 下一级模块向统一骨架对齐时的模块级检查列。每个模块只有在骨架完整性、职责分离、构建切换、验证入口和 bridge 退出五类条件同时满足后，才允许宣告完成统一骨架对齐。

## Checklist Columns

| Column | 含义 |
|---|---|
| `Skeleton ready` | `module.yaml`、`domain/`、`services/`、`application/`、`adapters/`、`tests/`、`examples/` 已存在；运行态模块可额外启用 `runtime/` |
| `Boundary clarified` | README、`module.yaml`、contracts 或冻结文档已说明哪些内容属于该模块 owner，哪些是迁出对象 |
| `Impl migrated` | owner 实现已迁入统一骨架，而不是继续以 `src/` 作为默认主实现来源 |
| `Build switched` | `CMakeLists.txt` 已以统一骨架目录为主，bridge 目录不再承担默认主构建入口 |
| `Tests/examples wired` | 模块内 `tests/` 与 `examples/` 已成为正式承载面，并能回链样例与验证 |
| `Bridge downgraded` | `src/` 或其他历史子树仅剩 bridge、wrapper、README 或 tombstone，不再接收新增终态代码 |
| `Closeout evidence` | `wave-mapping.md`、模块 README、系统验收报告或模块清单已记录 closeout 证据 |

## Module Matrix

| Module | Target skeleton | Special constraints | Skeleton ready | Boundary clarified | Impl migrated | Build switched | Tests/examples wired | Bridge downgraded | Closeout evidence | Current status |
|---|---|---|---|---|---|---|---|---|---|---|
| `workflow` | `README + module.yaml + contracts + domain + services + application + adapters + tests + examples` | `process-runtime-core/` 不得继续作为默认终态 owner 面 | `Completed` | `Completed` | `Completed (canonical-only implementation)` | `Completed (canonical build roots only; bridge targets consume canonical roots)` | `Completed` | `Completed (shell-only bridges)` | `Completed` | `Completed (canonical-only implementation; shell-only bridges)` |
| `job-ingest` | `README + module.yaml + contracts + domain + services + application + adapters + tests + examples` | 不应把宿主 CLI 逻辑混入模块内部 | `Completed` | `Completed` | `Completed (bridge-backed)` | `Completed (metadata + bridge)` | `Completed` | `Completed` | `Completed` | `Completed (bridge-backed)` |
| `dxf-geometry` | `README + module.yaml + contracts + domain + services + application + adapters + tests + examples` | `engineering-data/` 内容需按 owner 语义决定归位或外置 | `Completed` | `Completed` | `Completed (bridge-backed)` | `Completed (metadata + bridge)` | `Completed` | `Completed` | `Completed` | `Completed (bridge-backed)` |
| `topology-feature` | `README + module.yaml + contracts + domain + services + application + adapters + tests + examples` | 不得把几何解析实现继续混入特征 owner | `Completed` | `Completed` | `Completed (canonical-only implementation)` | `Completed (canonical module target only)` | `Completed` | `Completed (shell-only bridges)` | `Completed` | `Completed (canonical-only implementation; shell-only bridges)` |
| `process-planning` | `README + module.yaml + contracts + domain + services + application + adapters + tests + examples` | 模块内规划规则必须从宽泛 `src/` 中拆出 | `Completed` | `Completed` | `Completed (canonical-only implementation)` | `Completed (canonical configuration target at module root)` | `Completed` | `Completed (shell-only bridges)` | `Completed` | `Completed (canonical-only implementation; shell-only bridges)` |
| `coordinate-alignment` | `README + module.yaml + contracts + domain + services + application + adapters + tests + examples` | 对位补偿与设备控制边界必须明确 | `Completed` | `Completed` | `Completed (canonical-only implementation)` | `Completed (canonical module target only)` | `Completed` | `Completed (shell-only bridges)` | `Completed` | `Completed (canonical-only implementation; shell-only bridges)` |
| `process-path` | `README + module.yaml + contracts + domain + services + application + adapters + tests + examples` | 不得让下游模块反向重排路径事实 | `Completed` | `Completed` | `Completed (canonical-only implementation)` | `Completed (canonical module target only)` | `Completed` | `Completed (shell-only bridges)` | `Completed` | `Completed (canonical-only implementation; shell-only bridges)` |
| `motion-planning` | `README + module.yaml + contracts + domain + services + application + adapters + tests + examples` | 设备控制、回零、手动控制和运行态接口不得未经评审直接留在 `M7` | `Completed` | `Completed` | `Completed (bridge-backed; residual runtime/control surface frozen)` | `Completed (canonical owner target active; residual bridge-backed runtime/control surface frozen)` | `Completed` | `Completed` | `Completed` | `Completed (bridge-backed; canonical owner target active; residual runtime/control surface frozen)` |
| `dispense-packaging` | `README + module.yaml + contracts + domain + services + application + adapters + tests + examples` | 不得吸收运行态执行与回退控制语义 | `Completed` | `Completed` | `Completed (canonical-only implementation)` | `Completed (canonical module target only)` | `Completed` | `Completed (shell-only bridges)` | `Completed` | `Completed (canonical-only implementation; shell-only bridges)` |
| `runtime-execution` | `README + module.yaml + contracts + domain + services + application + adapters + runtime + tests + examples` | 允许 `runtime/` 作为受控特化；宿主壳与设备适配必须分层 | `Completed` | `Completed` | `Completed (canonical-only implementation)` | `Completed (canonical build roots only; bridges are forwarders)` | `Completed` | `Completed (shell-only bridges)` | `Completed` | `Completed (canonical-only implementation; shell-only bridges)` |
| `trace-diagnostics` | `README + module.yaml + contracts + domain + services + application + adapters + tests + examples` | 不得反向修正上游 owner 事实 | `Completed` | `Completed` | `Completed (canonical-only implementation)` | `Completed (canonical build roots only)` | `Completed` | `Completed (shell-only bridges)` | `Completed` | `Completed (canonical-only implementation; shell-only bridges)` |
| `hmi-application` | `README + module.yaml + contracts + domain + services + application + adapters + tests + examples` | `apps/hmi-app/` 只能保留宿主职责，不能继续承载业务 owner | `Completed` | `Completed` | `Completed (bridge-backed)` | `Completed (metadata + bridge)` | `Completed` | `Completed` | `Completed` | `Completed (bridge-backed)` |

## Wave Checkpoints

| Wave | Scope | Entry gate | Exit gate | Status |
|---|---|---|---|---|
| `Wave 0` | 规则与骨架顶层落位 | 已确认统一骨架与 bridge 语义 | 所有模块具备统一骨架顶层目录 | `Completed` |
| `Wave 1` | `M1-M8` 规划链模块实现迁移 | `Wave 0` 完成 | 上游与规划链模块主实现已迁入统一骨架 | `Completed (bridge-backed)` |
| `Wave 2` | `M0 workflow` 职责净化 | `Wave 1` 完成 | `workflow` 不再以历史聚合目录作为默认终态结构 | `Completed (bridge-backed)` |
| `Wave 3` | `M9-M11` 高复杂模块收敛 | `Wave 2` 完成 | 运行态与宿主耦合模块完成统一骨架对齐 | `Completed (bridge-backed)` |
| `Wave 4` | 全模块构建切换与 bridge 退出 | `Wave 3` 完成 | 所有模块 bridge 退出主实现来源 | `Completed (bridge-backed)` |

## Shared Rules

| Rule | 说明 |
|---|---|
| `New code goes to target skeleton` | 一旦模块具备统一骨架顶层，新增终态代码不得继续放入 `src/` |
| `Contracts first` | 需要跨模块暴露的稳定接口必须先进入 `contracts/` 或 `shared/contracts/` |
| `Bridge is temporary` | `src/` 及类似历史子树只允许作为迁移桥，不构成终态许可 |
| `Evidence-based closeout` | 没有模块级证据回链的“完成”不成立 |

补充说明：

- 本清单中的 `Completed (bridge-backed)` 表示统一骨架、元数据和主实现来源切换已经完成，但兼容桥可能仍保留 fallback CMake、wrapper 或历史实现。
- `runtime-execution` 的 `shell-only bridges` 表示历史 bridge 根仅保留 forwarder / README / marker，不再承载真实 `.h/.cpp` payload。
- `workflow` 的 `shell-only bridges` 表示 `src` 与 `process-runtime-core` 两个 bridge roots 已物理排空真实实现，仅保留 README、边界文档、兼容 CMake 与测试 forwarder。
- `trace-diagnostics`、`topology-feature`、`coordinate-alignment` 的 `shell-only bridges` 表示 `src` 根仅保留 `README.md`。
- `process-planning` 的 `shell-only bridges` 表示 `src` 根已物理排空到只剩 `README.md`。
- `process-path` 与 `dispense-packaging` 的 `shell-only bridges` 表示 `src` 根已物理排空到只剩 `README.md`，模块根 target 已切到各自 canonical 子域 target。
- `motion-planning` 的 `canonical owner target active` 表示模块根 target 已稳定转发到 canonical `siligen_motion`，且 `workflow` / canonical tests 已通过该 owner target 闭环构建。
- `motion-planning` 的 `residual runtime/control surface frozen` 表示本阶段仅冻结 residual 范围，不宣称 `src` 已进入 shell-only closeout。
- `Completed (bridge-backed)` 不是 `Final hard closeout`；后者要求 bridge 子树被物理排空、删除或仅剩 tombstone。

