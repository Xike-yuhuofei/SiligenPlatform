# NOISSUE 路径硬绑定与门禁基线清单 - 整仓目录重构前置准备

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 关联范围文档：NOISSUE-scope-20260322-130211.md
- 关联架构文档：NOISSUE-arch-plan-20260322-130211.md
- 关联映射文档：NOISSUE-dir-map-20260322-130211.md

## 1. 统计口径

本清单中的“命中文件数”来自本地 `rg -l --fixed-strings` 粗量搜索，统计的是“执行硬绑定与门禁引用”的粗量规模，不等同于源代码真实实现面。

已排除：

1. `third_party/**`
2. `build/**`
3. `tmp/**`
4. `docs/process-model/plans/**`

命中文件数用于评估重构成本与波次优先级，不代表精确行数或唯一调用点数量。
`integration/reports`、`data/recipes`、`config/` 这类路径还存在明显的“执行引用 + 文档引用 + 夹具引用”混合体，因此后续判断必须结合下文分类结论，而不能只看总命中数。
排除 `docs/process-model/plans/**` 的原因是本轮准备工件会主动引用这些路径；若不排除，会把计划文档自身误计入“真实硬绑定”。

## 2. 高优先级硬绑定（按命中文件数）

| 路径/入口 | 命中文件数 | 结论 |
|---|---:|---|
| `packages/process-runtime-core` | 58 | 当前目录重构的一号高风险 owner |
| `packages/simulation-engine` | 46 | 独立支撑能力，当前不宜强行并入业务模块 |
| `packages/runtime-host` | 43 | 实际硬绑定主要在 build/test 与运行资产默认解析，不是大量字面路径调用 |
| `packages/transport-gateway` | 37 | app boundary、tests、docs、CMake 多面绑定 |
| `packages/device-adapters` | 27 | 设备适配与 vendor 资产耦合深 |
| `packages/engineering-data` | 25 | 真正执行绑定主要集中在 3 个默认入口，不是泛化大面积路径调用 |
| `packages/shared-kernel` | 23 | 根级 tests/CMake 与多 package 直接依赖 |
| `data/recipes` | 22 | 命中中混入历史记录与说明文本，实际执行绑定低于粗量统计 |
| `config/machine/machine_config.ini` | 21 | 运行入口、runtime-host、文档与 CMake 默认值 |
| `packages/device-contracts` | 21 | device seam 与 tests/CMake 相关 |
| `packages/engineering-contracts` | 19 | schema/proto/兼容链相关 |
| `legacy-exit-check.ps1` | 16 | 当前所有目录切换都受其门禁保护 |
| `integration/reports` | 15 | test/CI/release/validation 的统一证据根，实际执行引用比粗量统计更强 |
| `packages/test-kit` | 13 | 根级 test runner 和报告模型直接依赖 |
| `packages/application-contracts` | 12 | app/gateway/HMI/CLI 相关 |
| `packages/traceability-observability` | 10 | runtime/device/logging 尚未完全收口 |
| `packages/engineering-data/scripts/dxf_to_pb.py` | 6 | DXF 离线链默认脚本路径 |
| `data/schemas/recipes` | 5 | 运行时 schema 默认路径 |

补充判断：

1. `integration/reports` 不应按单纯字符串命中视为低优先级，它是 `legacy-exit`、workspace validation 与 CI 的统一证据根，实际治理权重高于粗量文件数。
2. `data/recipes` 的粗量命中被历史数据与说明文本放大，不能把总命中数直接等同于运行时默认绑定强度。
3. `packages/runtime-host` 的重心在 build/test 与默认资产解析，字面路径绑定主要服务于构建和验证链，不应解读为源码中存在大量路径硬编码。
4. `packages/engineering-data` 的真实执行绑定集中在 `dxf_to_pb.py`、`path_to_trajectory.py`、`dxf_pipeline_path` 这 3 个默认入口，`simulation-engine` 当前更多是边界文档提及，不应视为同等级执行绑定面。

## 3. 硬绑定分类

### 3.1 Build / CMake 硬绑定

关键事实：

1. 根级 `CMakeLists.txt` 直接把以下路径写成 canonical source root 或资产路径：
   - `packages/runtime-host`
   - `packages/process-runtime-core`
   - `packages/shared-kernel`
   - `packages/transport-gateway`
   - `packages/traceability-observability`
   - `config/machine/machine_config.ini`
2. 根级 `tests/CMakeLists.txt` 直接聚合：
   - `packages/shared-kernel/tests`
   - `packages/process-runtime-core/tests`
   - `packages/runtime-host/tests`
3. `tools/build/build-validation.ps1` 明确按 suite 驱动 `packages/simulation-engine` 与 canonical control apps/tests。

重构含义：

1. 目录重构不是简单移动源码，必须同步重写根级 CMake 变量、`add_subdirectory(...)`、测试入口和构建产物目录。
2. `tests/` 顶层目录在 root CMake/source-root 迁出前不能删除或翻转。
3. `packages/simulation-engine` 当前应视为独立支撑能力，不应与业务模块 cutover 同波次处理。

### 3.2 测试 / CI / 报告目录硬绑定

关键事实：

1. `build.ps1`、`test.ps1`、`ci.ps1` 当前 suite 固定为：
   - `apps`
   - `packages`
   - `integration`
   - `protocol-compatibility`
   - `simulation`
2. `.github/workflows/workspace-validation.yml` 按这些 suite 矩阵执行，并把报告上传到 `integration/reports/ci/*`
3. `legacy-exit-check.ps1` 默认报告目录是 `integration/reports/legacy-exit`
4. `invoke-workspace-tests.ps1` 默认报告目录是 `integration/reports`
5. 这些路径在 Windows 下常以反斜杠出现，文本统计如果不统一归一化，会低估 `integration/reports` 的实际治理权重

重构含义：

1. 在 phase 0-2 前，不允许改写 suite 语义和默认报告根。
2. 任何目录切换都要保持 `integration/reports/*` 仍是统一证据根。
3. `legacy-exit-check.ps1` 是目录重构的硬门禁，不是可选脚本，且其报告链与 workspace validation 的证据链必须保持一致。

### 3.3 运行资产默认路径硬绑定

关键事实：

1. `apps/control-runtime`、`apps/control-tcp-server`、`apps/control-cli` 和 `packages/runtime-host` 都直接使用 `config/machine/machine_config.ini`
2. `packages/runtime-host` 默认解析：
   - `data/recipes`
   - `data/schemas/recipes`
3. `packages/transport-gateway` 的默认配置路径仍指向 `config/machine/machine_config.ini`

重构含义：

1. `config/`、`data/` 当前是运行时真实 contract，不能回卷到候选 `shared/config` 或 `samples/recipes` 语义。
2. 如果目录重构要改这些路径，必须先提供 bridge 与回滚策略；本轮准备阶段不允许做该类改动。
3. 对 `packages/runtime-host` 而言，默认路径绑定主要发生在运行配置与数据发现链，不应被误判为源码中到处存在字面路径硬编码。

### 3.4 离线工程链与脚本硬绑定

关键事实：

1. `packages/process-runtime-core/src/application/services/dxf/DxfPbPreparationService.cpp` 默认调用 `packages/engineering-data/scripts/dxf_to_pb.py`
2. `packages/process-runtime-core/src/domain/configuration/ports/IConfigurationPort.h` 默认脚本路径为 `packages/engineering-data/scripts/path_to_trajectory.py`
3. `packages/process-runtime-core/src/infrastructure/adapters/planning/dxf/DXFMigrationConfig.h` 默认把 `packages/engineering-data` 作为 `dxf_pipeline_path`
4. `packages/engineering-data` 的真实执行绑定面主要集中在上述 3 个默认入口，`simulation-engine` 不是同一量级的执行绑定点

重构含义：

1. `packages/engineering-data` 不能在 phase 0-2 被简单改名或移动。
2. 离线工程链需要单独的 path bridge 或同波次脚本迁移，不能与 runtime-execution 混合处理。
3. 后续若要下沉 `packages/engineering-data`，必须先处理这 3 个入口的 bridge 或替换，而不是单看目录命中数。

### 3.5 Legacy / control-core 阻塞绑定

关键事实：

1. `compatibility-shell-audit.md` 明确指出：
   - `packages/process-runtime-core` 仍依赖 `control-core/src/shared`、`control-core/src/infrastructure/adapters/planning/dxf`、`control-core/third_party`
   - `packages/device-adapters` / `packages/device-contracts` 仍依赖 `control-core/modules/device-hal` 或 `control-core/third_party`
2. `control-core-provenance-audit.md` 明确指出后续阻塞项收敛为：
   - `src/shared`
   - `src/infrastructure`
   - `modules/device-hal`
   - `third_party`
   - 顶层 CMake/source-root owner

重构含义：

1. wave 2 以前，任何涉及 `packages/process-runtime-core`、`packages/device-adapters`、`packages/device-contracts` 的迁移都必须先完成 residual dependency 冻结。
2. 目录重构不能被误当成“绕过 `control-core` 阻塞项”的捷径。

## 4. 典型硬绑定实例（必须纳入 cutover 检查）

| 类别 | 典型文件 | 当前硬绑定 | 检查要求 |
|---|---|---|---|
| CMake source root | `CMakeLists.txt` | `packages/runtime-host` / `packages/process-runtime-core` / `packages/transport-gateway` / `packages/shared-kernel` | cutover 前后都必须能解析正确 source root |
| 根级测试聚合 | `tests/CMakeLists.txt` | `../packages/*/tests` | 在 root CMake/source-root 迁出前禁止删改 `tests/` 聚合职责 |
| 运行配置 | `apps/control-runtime/main.cpp`、`packages/runtime-host/src/bootstrap/InfrastructureBindingsBuilder.cpp` | `config/machine/machine_config.ini` | 改目录前必须保留默认配置发现能力 |
| 运行数据 | `packages/runtime-host/src/bootstrap/InfrastructureBindingsBuilder.cpp` | `data/recipes` / `data/schemas/recipes` | 改目录前必须保留默认数据发现能力 |
| 离线脚本 | `packages/process-runtime-core/src/application/services/dxf/DxfPbPreparationService.cpp` | `packages/engineering-data/scripts/dxf_to_pb.py` | 必须提供 path bridge 或同波次脚本迁移 |
| CI 报告根 | `workspace-validation.yml`、`legacy-exit-check.ps1`、`invoke-workspace-tests.ps1` | `integration/reports/*` | 报告目录不能丢失或分裂，且在 Windows 路径样式下仍必须被识别为同一治理根 |

## 5. 进入 wave 1 / wave 2 前的检查清单

### 5.1 wave 1 前

1. `shared/*` 的 contracts / kernel / testing 命名与导出方式已经冻结。
2. `packages/*` 中将迁向 `shared/*` 的 owner 均已登记到目录映射矩阵。
3. `legacy-exit-check.ps1`、`build.ps1`、`test.ps1`、`ci.ps1` 的当前基线已采集。

### 5.2 wave 2A 前

1. `packages/engineering-data` 的默认脚本与 CLI 路径清单已完成。
2. 离线工程链迁移不影响 `config/`、`data/`、`examples/`、`integration/reports/`。
3. `packages/engineering-contracts` 与 `packages/application-contracts` 的 shared 归宿已先冻结。
4. `packages/engineering-data` 的 3 个默认入口已给出 bridge 或替换方案。

### 5.3 wave 2B 前

1. `packages/process-runtime-core`、`packages/runtime-host`、`packages/device-adapters`、`packages/traceability-observability` 的目标模块边界已冻结。
2. `control-core` residual dependency 已逐项登记，不存在未解释引用。
3. app dry-run、suite 基线、`legacy-exit` 基线均为绿色或处于既有已知失败口径。

## 6. 本清单的直接用途

1. 用于切分目录重构任务，决定先做哪一波、不该动哪一波。
2. 用于设计搜索门禁，防止旧路径和未来路径同时成为默认入口。
3. 用于评估每个目录切换的系统性影响，而不是把目录重构误判为纯文档整理。
