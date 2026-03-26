# Implementation Plan: 工作区模板化架构重构

**Branch**: `refactor/arch/NOISSUE-architecture-refactor-spec` | **Date**: 2026-03-24 | **Spec**: `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\spec.md`  
**Working Folder**: `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\`  
**Input**: `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\spec.md` 中已冻结的需求/约束，以及当前会话中已确认的“owner-first 波次迁移”方案设计结论

**Note**: 本计划文件是本次任务的正式工作基线。当前目录独立于既有 `NOISSUE-architecture-refactor-spec/`；旧目录仅作为参考，不再作为本次任务的正式输入源。

## Summary

本次工作不是对现有仓库做路径重命名，而是以 `docs/architecture/dsp-e2e-spec/` 作为唯一正式冻结文档集与架构对齐基线，对当前工作区执行一次单仓多模块的原地架构迁移。

选定方案为：**owner-first 波次迁移**。其核心不是一次性总切换，也不是长期保留双轨结构，而是让 `modules/`、`shared/`、`apps/`、`tests/`、`samples/`、`scripts/` 逐波成为真实承载面，再在最后阶段统一切换根级构建图并退出旧根。

该方案的目标是同时满足四个结果：

1. `modules/` 成为 `M0-M11` 的唯一业务 owner 根。
2. `apps/` 只保留进程入口、装配与发布职责。
3. `shared/` 只保留稳定公共契约与低业务含义基础设施。
4. `packages/`、`integration/`、`tools/`、`examples/` 只作为迁移来源，最终被排空、降级或移除，不再作为终态 owner 面。

为保证实现质量，本计划不再把整个迁移压缩为少数粗粒度故事，而是按可独立验收的风险边界拆为 7 个用户故事，并与 `Wave 0-6` 形成显式映射。

## Technical Context

**Language/Version**: Markdown 文档；PowerShell 7；Python 3.11；C++17；CMake 3.20+  
**Primary Dependencies**: 根级 `build.ps1` / `test.ps1` / `ci.ps1`；`cmake/workspace-layout.env`；当前 CMake workspace target graph；protobuf；spdlog；既有 control/runtime 应用入口  
**Storage**: Git 跟踪的仓库文件系统资产；`config/`、`data/`、`docs/`、`samples/`、`tests/` 等根级目录；不引入新数据库  
**Testing**: 根级 `.\build.ps1`、`.\test.ps1`、`.\ci.ps1`；`tools/scripts/run-local-validation-gate.ps1`；Python `test_kit.workspace_validation`；C++/CTest；integration/e2e/performance 回归验证  
**Target Platform**: Windows-first 的控制运行环境与 CI 工作区  
**Project Type**: 单仓多模块的工作区级原地架构重构  
**Performance Goals**: 保持现有运行时链路行为与实时性；closeout 后不得因重构引入新的关键路径运行时跳转；保持根级构建/测试入口稳定可用  
**Constraints**: 必须原地重构；冻结条件不得擅自修改；`docs/architecture/dsp-e2e-spec/` 是唯一正式事实源；根级入口不得变更为新的外部调用方式；兼容桥必须显式且有退出波次；迁移期间不得形成长期双事实源  
**Scale/Scope**: 全工作区，覆盖 `apps/`、`modules/`、`shared/`、`packages/`、`integration/`、`tools/`、`examples/`、`tests/`、`samples/`、`scripts/`、`deploy/`、`docs/`、`config/`、`data/`、`cmake/` 与根级构建脚本

## Constitution Check

*GATE: Must pass before implementation task breakdown. Re-check before each migration wave starts.*

- [x] Branch name is compliant with `<type>/<scope>/<ticket>-<short-desc>`
- [x] Planned paths use canonical workspace roots；`modules/`、`shared/`、`tests/`、`samples/`、`scripts/`、`deploy/` 已由冻结文档定义为正式根
- [x] Build/test validation approach is explicit via root entry points (`.\build.ps1`, `.\test.ps1`, `.\ci.ps1`) and local validation gate
- [x] Compatibility/fallback behavior is explicit and time-bounded if legacy paths are involved

## Current Baseline

当前仓库处于“新旧并存、但旧根仍承载真实实现”的混合状态：

- `modules/`、`shared/`、`apps/planner-cli`、`apps/runtime-service`、`apps/runtime-gateway` 已存在，但多为桥接壳层。
- 根级 `CMakeLists.txt` 仍直接依赖 `packages/shared-kernel`、`packages/process-runtime-core`、`packages/runtime-host`、`packages/transport-gateway`。
- `apps/CMakeLists.txt` 仍显式构建 `control-runtime`、`control-tcp-server`、`control-cli` 等旧入口。
- `tests/CMakeLists.txt` 仍直接转向 `packages/*/tests`。
- `build.ps1`、`test.ps1` 仍经由 `tools/` 内部脚本驱动。

因此，设计重点不是“是否引入新架构”，而是“如何让现有壳层逐步变成真实 owner 承载面，并让旧根安全退出”。

## Design Goals

1. 让目录结构和工程语义同时完成对齐，而不是只完成路径迁移。
2. 以 `M0-M11` 为唯一拆分轴，按 owner 对象与模块边界重组现有实现。
3. 保持根级构建、测试与 CI 对外入口不变。
4. 将运行时和设备侧风险后置并隔离，避免与规划链静态迁移混在同一波次。
5. 为每一波定义明确的 bridge 形态、退出条件和验证闭环。

## Project Structure

### Documentation (this feature)

```text
D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\
├── README.md
├── spec.md
├── plan.md
├── tasks.md
├── wave-mapping.md
├── module-cutover-checklist.md
└── validation-gates.md
```

### Source Code (target repository root)

```text
D:\Projects\SS-dispense-align\
├── apps\
│   ├── hmi-app\
│   ├── planner-cli\
│   ├── runtime-service\
│   ├── runtime-gateway\
│   └── trace-viewer\
├── modules\
│   ├── workflow\
│   ├── job-ingest\
│   ├── dxf-geometry\
│   ├── topology-feature\
│   ├── process-planning\
│   ├── coordinate-alignment\
│   ├── process-path\
│   ├── motion-planning\
│   ├── dispense-packaging\
│   ├── runtime-execution\
│   ├── trace-diagnostics\
│   └── hmi-application\
├── shared\
│   ├── kernel\
│   ├── ids\
│   ├── artifacts\
│   ├── contracts\
│   ├── commands\
│   ├── events\
│   ├── failures\
│   ├── messaging\
│   ├── logging\
│   ├── config\
│   └── testing\
├── docs\
├── samples\
├── tests\
├── scripts\
├── config\
├── data\
├── deploy\
├── cmake\
├── third_party\
├── build\
├── logs\
├── uploads\
└── specs\

# Migration-source roots to be drained before closeout
├── packages\
├── integration\
├── tools\
└── examples\
```

**Structure Decision**: 采用“新根先变真、旧根后变壳、最后统一切换”的 owner-first 波次迁移结构。`modules/` 是唯一业务 owner 面；`apps/` 只保留进程入口；`shared/` 只保留稳定公共层；`tests/`、`samples/`、`scripts/` 作为正式承载面；旧根只在迁移期间存在，不保留终态双轨。

## User Story Decomposition

| User Story | Focus | Primary Wave Mapping | Independent Value |
|---|---|---|---|
| `US1` | 统一架构冻结基线与术语锁定 | `Wave 0` + 冻结文档基线 | 让所有后续讨论使用同一阶段链、术语和失败语义 |
| `US2` | canonical roots、共享支撑面与根治理 | `Wave 1` | 让 `shared/`、`scripts/`、`samples/`、`tests/`、`deploy/` 成为可承接迁移的正式根 |
| `US3` | 上游静态工程链 `M1-M3` | `Wave 2` | 稳定规划链输入事实与样本来源 |
| `US4` | 工作流与规划链 `M0`、`M4-M8` | `Wave 3` | 将混合的 `process-runtime-core` 拆成真实 owner 模块 |
| `US5` | 运行时执行链 `M9` 与入口壳 | `Wave 4` | 隔离实时链路风险并完成运行时 owner 收敛 |
| `US6` | 追溯、HMI 与验证承载面 `M10-M11` | `Wave 5` | 让 trace/HMI/tests/samples 成为正式终态承载面 |
| `US7` | 根级构建图切换与 legacy exit | `Wave 6` | 将 canonical roots 变为唯一真实构建图并关闭旧根默认地位 |

## Target Root Mapping

| Current surface | Target surface | Intent |
|---|---|---|
| `apps/control-cli/` | `apps/planner-cli/` + `modules/hmi-application/` + `modules/job-ingest/` | CLI 仅保留入口；任务创建、审批、查询与展示逻辑回到模块 owner |
| `apps/control-runtime/` + `packages/runtime-host/` | `apps/runtime-service/` + `modules/runtime-execution/` | 宿主/启动面与运行时事实 owner 分离 |
| `apps/control-tcp-server/` + `packages/transport-gateway/` | `apps/runtime-gateway/` + `modules/runtime-execution/adapters/` | 协议接入和执行 owner 归位，网关只保留进程入口 |
| `packages/shared-kernel/` | `shared/kernel/`、`shared/ids/`、`shared/artifacts/`、`shared/messaging/`、`shared/logging/` | 统一稳定公共层 |
| `packages/application-contracts/` | `modules/job-ingest/contracts/`、`modules/hmi-application/contracts/`、`shared/contracts/application/` | 应用侧契约按 owner 与跨模块稳定性拆分 |
| `packages/engineering-contracts/` | `shared/contracts/engineering/` + `data/` | 稳定 schema 与运行资产分离 |
| `packages/engineering-data/` | `modules/dxf-geometry/` + `modules/topology-feature/` | 几何解析与拓扑/特征 owner 分离 |
| `packages/process-runtime-core/` | `modules/workflow/`、`modules/process-planning/`、`modules/coordinate-alignment/`、`modules/process-path/`、`modules/motion-planning/`、`modules/dispense-packaging/` | 按 `M0`、`M4-M8` 真正拆分 owner |
| `packages/device-contracts/` + `packages/device-adapters/` | `modules/runtime-execution/contracts/` + `modules/runtime-execution/adapters/` + `shared/contracts/device/` | 设备契约与执行适配统一回收至运行时链 |
| `packages/traceability-observability/` | `modules/trace-diagnostics/` + `shared/logging/` | 追溯 owner 与通用日志基础设施分离 |
| `packages/test-kit/` | `shared/testing/` | 根级验证支撑下沉到正式公共层 |
| `packages/simulation-engine/` | `modules/runtime-execution/adapters/simulation/` + `tests/performance/` + `samples/` | 不再保留为独立 owner 包 |
| `integration/` | `tests/` | 跨模块验证收敛到正式测试根 |
| `tools/` | `scripts/` | 根级自动化收敛到正式脚本根 |
| `examples/` | `samples/` | 样本与 golden cases 收敛到正式样本根 |

## Design Principles

### 1. Owner-first

所有迁移都以“谁拥有业务事实”为先，而不是以“旧目录名是什么”为先。

### 2. Bridge is transitional

桥接层只允许用于兼容构建、include、脚本入口和过渡期引用；桥接层不得承载新业务逻辑。

### 3. Runtime isolation

`runtime-execution`、网关、设备侧适配和运行态状态机必须单独成波，不能与静态规划链一起大范围切换。

### 4. Root entry stability

无论内部如何迁移，根级 `build.ps1`、`test.ps1`、`ci.ps1` 对外保持稳定。

### 5. Single terminal fact source

同一类资产在 closeout 后只能有一个正式承载根；禁止长期双写、双构建、双 owner。

## Migration Waves

### Wave 0: Governance Freeze and Migration Inventory

目标：

- 固化本计划与冻结文档的一致性。
- 建立模块级迁移盘点、旧根到新根的映射索引和桥接规则。
- 明确旧根从此只允许兼容修复，不允许继续新增业务逻辑。

交付：

- 模块迁移清单
- 根级处置表
- bridge 规则表
- 每波的验收与退出门禁

### Wave 1: Shared / Scripts / Test Support

目标：

- 抽离 `shared-kernel`、`*-contracts`、`test-kit` 中真正稳定且低业务含义的公共层。
- 将 `tools/` 中被根级脚本调用的自动化逻辑迁入 `scripts/`。
- 保持根级 `build/test/ci` 接口不变，但内部开始指向新正式脚本根。

范围：

- `packages/shared-kernel/`
- `packages/application-contracts/`
- `packages/engineering-contracts/`
- `packages/device-contracts/`
- `packages/test-kit/`
- `tools/`

退出条件：

- `shared/` 成为稳定公共契约与测试支撑的真实承载面。
- `scripts/` 成为根级自动化真实承载面。
- `tools/` 只剩 wrapper、forwarder 或迁移期兼容脚本。

### Wave 2: Upstream Static Engineering Chain (`M1-M3`)

目标：

- 先迁移低实时风险、静态工程特征明显的上游链路。
- 使 `modules/job-ingest/`、`modules/dxf-geometry/`、`modules/topology-feature/` 成为真实 owner。

范围：

- `apps/hmi-app` 中属于任务接入与展示外壳之外的业务逻辑
- `apps/control-cli` 中属于任务创建、输入校验、查询的业务逻辑
- `packages/engineering-data/`
- 与上游链路紧耦合的契约和样本

退出条件：

- `M1-M3` 的 owner 实现不再以 `packages/` 为主承载面。
- 对应样本、fixtures、golden 输入已迁入 `samples/`。
- 对应测试已在 `tests/` 或模块内 `tests/` 中有正式落位。

### Wave 3: Workflow and Planning Chain (`M0`, `M4-M8`)

目标：

- 将 `packages/process-runtime-core/` 从混合核心拆成真实模块 owner。
- 让 `workflow`、`process-planning`、`coordinate-alignment`、`process-path`、`motion-planning`、`dispense-packaging` 各自独立承载事实与边界。

范围：

- `packages/process-runtime-core/src/application`
- `packages/process-runtime-core/planning`
- `packages/process-runtime-core/machine`
- `packages/process-runtime-core/src/domain`
- `packages/process-runtime-core/src/domain/motion`
- `packages/process-runtime-core/src/domain/dispensing`
- `packages/process-runtime-core/src/application/usecases/dispensing`

退出条件：

- `packages/process-runtime-core/` 不再作为 `M0`、`M4-M8` 的真实 owner 根。
- 新模块之间的依赖只通过 `shared/` 和对方 `contracts/` 发生。
- `ExecutionPackageBuilt` / `ExecutionPackageValidated` 等冻结语义未被破坏。

### Wave 4: Runtime Execution Chain (`M9`)

目标：

- 单独处理对实时性、设备接入和恢复语义最敏感的运行时链路。
- 让 `modules/runtime-execution/` 成为真实 owner。
- 让 `apps/runtime-service/` 与 `apps/runtime-gateway/` 变成真实入口。

范围：

- `packages/runtime-host/`
- `packages/device-contracts/` 中运行态相关部分
- `packages/device-adapters/`
- `packages/transport-gateway/`
- `apps/control-runtime/`
- `apps/control-tcp-server/`

退出条件：

- `runtime-service`、`runtime-gateway` 成为正式入口。
- `control-runtime`、`control-tcp-server` 仅剩兼容壳或退出。
- 设备执行链、预检、恢复、首件等时序不因目录重构改变语义。

### Wave 5: HMI / Trace / Validation Surface (`M10-M11`)

目标：

- 收尾 `trace-diagnostics` 与 `hmi-application` 的 owner 归位。
- 完成 `integration/`、`examples/` 向 `tests/`、`samples/` 的正式收敛。

范围：

- `packages/traceability-observability/`
- `apps/hmi-app/`
- `apps/control-cli/`
- `integration/`
- `examples/`

退出条件：

- `trace-diagnostics` 成为追溯事实 owner。
- `tests/` 成为唯一仓库级验证承载根。
- `samples/` 成为唯一稳定样本承载根。

### Wave 6: Root Build-Graph Cutover and Legacy Exit

目标：

- 统一修改根级 `CMakeLists.txt`、`apps/CMakeLists.txt`、`tests/CMakeLists.txt` 和 `cmake/workspace-layout.env` 的默认指向。
- 让新根成为唯一真实构建图。
- 关闭或移除旧根的默认承载地位。

退出条件：

- 根级构建图不再以 `packages/*` 为默认真实实现来源。
- `packages/`、`integration/`、`tools/`、`examples/` 只剩 tombstone、README、wrapper 或被彻底移除。
- 根级构建/测试/CI 与 freeze gate 全部通过。

## Wave Dependency Order

高层依赖顺序固定如下：

```text
Wave 0
  -> Wave 1
      -> Wave 2
          -> Wave 3
              -> Wave 4
                  -> Wave 5
                      -> Wave 6
```

补充说明：

- `Wave 0` 是所有后续波次的治理前提，没有它就无法定义桥接、退出和验收口径。
- `Wave 1` 是所有业务迁移波次的基础，因为 `shared/`、`scripts/` 和测试支撑必须先成为正式承载面。
- `Wave 2` 先稳定 `M1-M3`，为 `Wave 3` 的 `workflow` 与规划链提供上游 owner 和输入事实。
- `Wave 3` 完成后，`Wave 4` 才能在不回写上游规划对象的前提下处理运行时链。
- `Wave 5` 依赖 `Wave 2` 到 `Wave 4` 已经稳定，因为 `hmi-application`、`trace-diagnostics` 和仓库级验证面都需要引用前面波次的正式边界。
- `Wave 6` 只在前面所有 owner 与承载面都已经真实迁入新根后才允许执行。

## Wave Deliverable Summary

### Wave 0 Deliverables

- `wave-mapping.md`
- `validation-gates.md`
- 模块迁移盘点表
- bridge 规则表

### Wave 1 Deliverables

- `shared/` 下的正式公共层承载面
- `scripts/` 下的正式自动化承载面
- 根级入口保持不变但内部可切向新脚本根的桥接方案

### Wave 2 Deliverables

- `modules/job-ingest/`
- `modules/dxf-geometry/`
- `modules/topology-feature/`
- 与上游链路对应的样本、契约和测试正式落位

### Wave 3 Deliverables

- `modules/workflow/`
- `modules/process-planning/`
- `modules/coordinate-alignment/`
- `modules/process-path/`
- `modules/motion-planning/`
- `modules/dispense-packaging/`
- `process-runtime-core` 各 owner 的拆分结果与依赖收敛结果

### Wave 4 Deliverables

- `modules/runtime-execution/`
- `apps/runtime-service/`
- `apps/runtime-gateway/`
- 设备契约与适配回收到运行时链的结果
- 旧运行时入口的兼容壳或退出结论

### Wave 5 Deliverables

- `modules/trace-diagnostics/`
- `modules/hmi-application/`
- `tests/` 正式验证承载面
- `samples/` 正式样本承载面
- `integration/`、`examples/` 的退出结果

### Wave 6 Deliverables

- 切换后的根级 `CMakeLists.txt`
- 切换后的 `apps/CMakeLists.txt`
- 切换后的 `tests/CMakeLists.txt`
- 切换后的 `cmake/workspace-layout.env`
- legacy roots closeout 结论与最终验收证据

## Bridge Strategy

### Build Bridge

- 新根先创建真实 target。
- 旧 target 在过渡期改为 `INTERFACE`、`ALIAS` 或薄转发 CMake。
- 不允许长期双 target 都承载真实实现。

### Source Bridge

- 旧 include 根只保留 forwarding header、兼容 include 或极薄适配层。
- 旧源码目录不得继续堆积新增业务实现。

### Script Bridge

- `build.ps1`、`test.ps1`、`ci.ps1` 对外不变。
- 内部调用逐步从 `tools/` 切换到 `scripts/`。
- 旧 `tools/` 只保留 wrapper，不再承载真实自动化逻辑。

### Validation / Report Bridge

- 迁移期间允许报告仍输出到旧兼容路径，但正式验证承载面必须逐步转向 `tests/` 和 `docs/validation/`。
- closeout 后不得保留双报告根作为默认事实源。

## Validation and Cutover Gates

每一波进入下一波前，至少满足以下门禁：

1. 该波范围内的新根已成为真实 owner 或真实承载面。
2. 旧根只剩兼容壳、README、转发脚本或待移除空骨架。
3. 根级 `.\build.ps1`、`.\test.ps1`、`.\ci.ps1` 仍可执行，且无新增外部使用方式。
4. 受影响模块的依赖方向符合 `apps -> modules -> shared` 或“模块仅依赖对方 contracts”的规则。
5. 冻结文档与验证基线未出现新的语义漂移。

## Key Risks and Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| `modules/` 长期停留在壳层，旧根继续写入真实实现 | 形成永久双事实源 | 每波都要求“新根先变真”，并禁止旧根承接新增业务 |
| 在拆 `process-runtime-core` 时按旧包整体搬运，而不是按 owner 拆分 | 新目录看似正确，边界实质未改变 | 以 `M0/M4-M8` 为拆分轴，不允许“整包平移后再解释” |
| 运行时链与静态规划链同时切换 | 故障半径过大，难以定位回归 | 将 `M9` 单独成波，后置处理 |
| 根级构建和测试入口过早切换 | CI 与本地使用方式中断 | 保持根级入口稳定，先内部换向，最后统一 cutover |
| `tools/`、`integration/`、`examples/` 长期双写 | 迁移无法 closeout | 明确正式承载面分别为 `scripts/`、`tests/`、`samples/`，旧根只保留过渡角色 |

## Complexity Tracking

无额外例外项。当前方案完全基于已冻结需求、已冻结文档和已确认推荐路径展开；不新增未批准的架构前提。
