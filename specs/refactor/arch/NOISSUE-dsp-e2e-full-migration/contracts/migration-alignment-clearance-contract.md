# Contract: 迁移归位与清除矩阵

## 1. 合同标识

- Contract ID: `migration-alignment-clearance-v1`
- Scope: `D:\Projects\SS-dispense-align\apps\`, `modules\`, `shared\`, `docs\`, `samples\`, `tests\`, `scripts\`, `config\`, `data\`, `deploy\`

## 2. 目的

定义本特性要求的权威迁移归位与清除矩阵格式，确保 `M0-M11` 与相关 canonical roots 资产都能被逐项验证是否已经完成 owner 面收敛、bridge 清除、根级验证接入和证据闭环。

## 3. 必填列

每一行至少必须包含以下字段：

| 列 | 说明 |
|---|---|
| `module_id` | `M0-M11` 或 `ROOT` |
| `canonical_root_ref` | `apps`、`modules`、`shared`、`docs`、`samples`、`tests`、`scripts`、`config`、`data`、`deploy` 之一 |
| `asset_family` | `implementation`、`build-entry`、`tests`、`contracts`、`samples`、`scripts`、`docs`、`config`、`data`、`deploy` |
| `target_owner_surface` | 终态 canonical 路径 |
| `current_live_surface` | 当前真实实现/真实构建/真实 owner 面 |
| `legacy_paths_to_clear` | 仍需清除的 bridge/source/fallback 路径或配置 |
| `required_gate_entries` | 必须证明该行关闭的 root gate 或 validation script |
| `required_reports` | 需要回链的报告路径 |
| `blocking_condition` | 什么情况下该行仍判定为未完成 |
| `status` | `identified`、`planned`、`migrating`、`cleared`、`verified`、`blocked` |

## 4. 模块覆盖要求

矩阵必须至少覆盖以下模块 owner 面：

| 模块 | 终态 owner 面 | 最低 bridge/legacy 检查面 |
|---|---|---|
| M0 `workflow` | `modules/workflow/{application,domain,adapters,services,tests,examples}` | `modules/workflow/src`, `modules/workflow/process-runtime-core`, `modules/workflow/application/usecases-bridge` |
| M1 `job-ingest` | `modules/job-ingest/{application,domain,adapters,services,tests,examples}` | `modules/job-ingest/src` |
| M2 `dxf-geometry` | `modules/dxf-geometry/{application,domain,adapters,services,tests,examples}` 与正式脚本入口 | `modules/dxf-geometry/src`, `modules/dxf-geometry/engineering-data`, `scripts/engineering-data-bridge` |
| M3 `topology-feature` | `modules/topology-feature/{application,domain,adapters,services,tests,examples}` | `modules/topology-feature/src` |
| M4 `process-planning` | `modules/process-planning/{application,domain,adapters,services,tests,examples}` | `modules/process-planning/src` |
| M5 `coordinate-alignment` | `modules/coordinate-alignment/{application,domain,adapters,services,tests,examples}` | `modules/coordinate-alignment/src` |
| M6 `process-path` | `modules/process-path/{application,domain,adapters,services,tests,examples}` | `modules/process-path/src` |
| M7 `motion-planning` | `modules/motion-planning/{application,domain,adapters,services,tests,examples}` | `modules/motion-planning/src` |
| M8 `dispense-packaging` | `modules/dispense-packaging/{application,domain,adapters,services,tests,examples}` | `modules/dispense-packaging/src` |
| M9 `runtime-execution` | `modules/runtime-execution/{contracts/device,adapters/device,runtime/host,domain,services,tests,examples}` | `modules/runtime-execution/runtime-host`, `modules/runtime-execution/device-adapters`, `modules/runtime-execution/device-contracts` |
| M10 `trace-diagnostics` | `modules/trace-diagnostics/{application,domain,adapters,services,tests,examples}` | `modules/trace-diagnostics/src` |
| M11 `hmi-application` | `modules/hmi-application/{application,domain,adapters,services,tests,examples}` | `modules/hmi-application/src` |

## 5. 跨根资产覆盖要求

矩阵还必须覆盖以下跨根资产族：

| 资产族 | 终态要求 |
|---|---|
| 根级入口 `build.ps1`、`test.ps1`、`ci.ps1` | 只能解析 canonical graph，不得依赖 bridge/fallback |
| `apps/*` 中与 `M0-M11` owner 直接相关的应用入口、默认启动脚本和默认消费链路 | 必须与对应模块 owner 面同轨收敛，不得继续通过 legacy 目录或桥接壳提供真实入口 |
| `shared/*` 中与 `M0-M11` owner 直接相关的共享契约、消息、配置、测试支持层 | 必须按真实 owner 关系归位，不得作为“模块外历史堆栈”逃逸出矩阵 |
| `scripts/migration/*` 与 `scripts/validation/*` | 必须以“拒绝 bridge 终态”为规则，而不是继续允许 bridge 存在 |
| `scripts/migration/validate_dsp_e2e_spec_docset.py` 与 `docs/architecture/dsp-e2e-spec/*` | 必须能够证明唯一冻结事实源完整可用 |
| `docs/architecture/workspace-baseline.md` 与 `docs/architecture/system-acceptance-report.md` | 必须与单轨 canonical roots 和零 legacy fallback 结论一致 |
| `tests/reports/*` | 必须提供可回链的 workspace validation、legacy-exit、local gate 证据 |
| `samples/`、`config/`、`data/`、`deploy/` | 若与模块 owner 直接相关，必须归位到对应 canonical owner 面并在矩阵中列出 |

## 6. 状态判定规则

| 状态 | 定义 |
|---|---|
| `identified` | 已确认目标 owner 面与待清除 legacy 面，但尚未形成实施动作 |
| `planned` | 已明确迁移步骤、负责方和 gate 证据 |
| `migrating` | 正在迁移或删除 bridge/legacy 资产 |
| `cleared` | bridge/legacy 资产已删除，但根级 gate 证据尚未补齐 |
| `verified` | 迁移、清除和 gate 证据全部完成 |
| `blocked` | 存在 live build/runtime/docs/gate 依赖仍指向 legacy 面 |

## 7. 阻断条件

任一条矩阵记录出现以下任一情况，都必须保持 `blocked`:

1. `current_live_surface` 仍等于或包含 `legacy_paths_to_clear`
2. 对应模块 `module.yaml` 仍保留 `bridge_mode: active`
3. 对应模块 `CMakeLists.txt` 仍保留 `SILIGEN_BRIDGE_ROOTS` 或 `SILIGEN_MIGRATION_SOURCE_ROOTS`
4. 根级或模块级脚本仍以 bridge/fallback 路径作为默认入口
5. 对应 `required_reports` 缺失，或报告结论未能证明该条目所要求的 proof dimension（例如 `zero-bridge-root`、`zero-legacy-root`、`freeze-docset-valid`、`root-gate-pass`）

## 8. Closeout 条件

矩阵只有在以下条件全部满足时，才可用于宣告本特性 closeout：

1. `M0-M11` 和跨根资产族全部具有矩阵记录。
2. 所有记录状态均为 `verified`。
3. 相关根级 gate 与 canonical docs 中的结论保持一致。
