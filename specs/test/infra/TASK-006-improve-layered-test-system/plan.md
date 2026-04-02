# Implementation Plan: Layered Test System

**Branch**: `test/infra/TASK-006-improve-layered-test-system` | **Date**: 2026-04-01 | **Spec**: `D:\Projects\ss-e\specs\test\infra\TASK-006-improve-layered-test-system\spec.md`
**Input**: Feature specification from `D:\Projects\ss-e\specs\test\infra\TASK-006-improve-layered-test-system\spec.md`

## Summary

在不新增平行测试根和默认执行入口的前提下，把仓库现有 `build.ps1`、`test.ps1`、`ci.ps1`、`shared/testing/test-kit`、`tests/integration/`、`tests/e2e/`、`tests/performance/` 收敛成一套分层测试体系。设计重点是三件事：用统一的 `ValidationLayer` 与 `ExecutionLane` 路由改动风险，用共享 `samples/`、`tests/baselines/`、`shared/testing/` 与故障场景支撑离线优先验证，并把所有门禁与专项验证都统一回收到 `tests/reports/` 的结构化证据口径。

## Technical Context

**Language/Version**: PowerShell 7（根级入口与 validation scripts）、Python 3.11（`shared/testing/test-kit`、validation/performance tooling）、C++17 / CMake 3.20+（canonical test roots 与 repo build graph）、Markdown（设计与验证文档）  
**Primary Dependencies**: `build.ps1` / `test.ps1` / `ci.ps1`；`scripts/validation/invoke-workspace-tests.ps1`；`scripts/validation/run-local-validation-gate.ps1`；`shared/testing/test-kit`；`python -m test_kit.workspace_validation`；`scripts/migration/validate_workspace_layout.py`；`tests/performance/collect_dxf_preview_profiles.py`  
**Storage**: Git 跟踪的 `tests/`、`samples/`、`docs/validation/`、`shared/contracts/` 资产，以及运行期输出目录 `tests/reports/`；无数据库  
**Testing**: 根级 `.\build.ps1`、`.\test.ps1`、`.\ci.ps1`；`python -m test_kit.workspace_validation`；仓库级承载面 `tests/integration/`、`tests/e2e/`、`tests/performance/`；`tests/CMakeLists.txt` 聚合的 canonical module test roots  
**Target Platform**: Windows 开发机与 CI 代理；默认离线 / 模拟验证；真实硬件 smoke 与 HIL 仅显式 opt-in  
**Project Type**: 工业自动化多模块工作区上的 spec-driven repository validation platform  
**Performance Goals**: 常规改动快速门禁在 15 分钟内返回；完整验证必须产出 canonical evidence；性能层能比较 `small / medium / large` DXF 基线与漂移  
**Constraints**: 只能使用 canonical workspace roots；必须遵循“先离线、后联机”；不得引入隐藏 fallback 或第二真值；失败必须输出结构化证据与必要 trace 字段；HIL 不能成为默认通过路径  
**Scale/Scope**: 覆盖 root scripts、`shared/testing/`、`tests/`、`samples/`、`docs/validation/`，以及 `workflow`、`job-ingest`、`dxf-geometry`、`process-path`、`motion-planning`、`dispense-packaging`、`runtime-execution` 和 `apps/runtime-service/tests` 等 canonical test surfaces

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] Branch name is compliant with `<type>/<scope>/<ticket>-<short-desc>`
- [x] Planned paths use canonical workspace roots; no new legacy default paths
- [x] Build/test validation approach is explicit via root entry points (`.\build.ps1`, `.\test.ps1`, `.\ci.ps1`) plus justified subsets (`scripts\validation\run-local-validation-gate.ps1`, `scripts\migration\validate_workspace_layout.py`, `python -m test_kit.workspace_validation`, `tests\performance\collect_dxf_preview_profiles.py`)
- [x] Compatibility/fallback behavior is explicit and time-bounded if legacy paths are involved (本特性不新增 legacy 路径；现有 `-IncludeHardwareSmoke`、`-IncludeHilClosedLoop`、`-IncludeHilCaseMatrix` 仍保持显式 opt-in，不构成默认通过链)

## Project Structure

### Documentation (this feature)

```text
D:\Projects\ss-e\specs\test\infra\TASK-006-improve-layered-test-system\
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts\
└── tasks.md
```

### Source Code (repository root)

```text
D:\Projects\ss-e\
├── build.ps1
├── test.ps1
├── ci.ps1
├── scripts\
│   ├── validation\
│   └── migration\
├── shared\testing\test-kit\
├── tests\
│   ├── baselines\
│   ├── contracts\
│   ├── integration\
│   ├── e2e\
│   │   ├── simulated-line\
│   │   └── hardware-in-loop\
│   ├── performance\
│   └── reports\
├── samples\
│   ├── dxf\
│   └── golden\
├── modules\
│   ├── workflow\tests\
│   ├── job-ingest\tests\
│   ├── dxf-geometry\tests\
│   ├── process-path\tests\
│   ├── motion-planning\tests\
│   ├── dispense-packaging\tests\
│   └── runtime-execution\tests\
├── apps\runtime-service\tests\
└── docs\
    ├── architecture\build-and-test.md
    └── validation\README.md
```

**Structure Decision**: 保持现有多模块 canonical workspace，不新增新的顶层测试根、平行 CI 入口或模块外 owner。分层测试体系通过扩展 `tests/` 仓库级承载面、`shared/testing/test-kit` 共用支撑层、`samples/` / `tests/baselines/` 事实资产，以及模块内 `*/tests` owner surfaces 实现。

## Post-Design Constitution Re-check

- [x] 设计仍只使用 `apps/`、`modules/`、`shared/`、`docs/`、`samples/`、`tests/`、`scripts/`、`config/`、`data/`、`deploy/` canonical roots
- [x] 根级 build/test/CI 入口仍是唯一默认执行面，新增设计没有引入平行 runner 或默认 ad-hoc path
- [x] 离线优先与 HIL 显式 opt-in 规则在设计中被保留，没有把 mock 或真实硬件路径提升为默认 owner 或默认成功链
- [x] `specs/` 仅作为 feature artifact 根存在；实现资产仍计划落回 canonical workspace roots

## Complexity Tracking

当前无宪章豁免项；本节保持空白。
