# Implementation Plan: HMI 胶点预览锚定分布一致性

**Branch**: `fix/workflow/SPEC-001-glue-point-alignment` | **Date**: 2026-03-29 | **Spec**: `D:\Projects\SiligenSuite\specs\fix\workflow\SPEC-001-glue-point-alignment\spec.md`
**Input**: Feature specification from `D:\Projects\SiligenSuite\specs\fix\workflow\SPEC-001-glue-point-alignment\spec.md`

## Summary

将计划胶点预览收敛为一条从 dispensing planning owner service 产生、同时被 execution package 与 preview snapshot 共用的权威结果链。实现重点是：在 owner planning 层按每条应出胶几何线段执行首尾双锚定和段内均匀分布，写回显式 `enable_position_trigger`，把 `trigger_distances_mm` 降级为派生兼容产物，移除 preview 对 `motion_trajectory_points`、距离回推和固定间隔采样的隐式 fallback，按真实几何分组复核 spacing，并保持 gateway/HMI 继续只接受 `planned_glue_snapshot + glue_points` 作为执行前门禁。

项目级补充：自 2026-03-31 起，DXF authority layout 的上位策略以 `D:\Projects\SiligenSuite\docs\architecture\dxf-authority-layout-global-strategy-v1.md` 为准。本文中的“按每条应出胶几何线段执行首尾双锚定”仅适用于归一化/分派后边界被判定为硬锚点，或路径被切分为独立 `open_chain` 的场景；它不应被解释为所有 DXF 默认按原始几何段独立布点。

## Technical Context

**Language/Version**: C++17 / CMake 3.20+（`modules/dispense-packaging`、`modules/workflow`、`apps/runtime-gateway`）、Python 3.11（`apps/hmi-app` 与 Python 协议/GUI 测试）、PowerShell 7（根级 `build.ps1` / `test.ps1` / `ci.ps1` 与 workflow 脚本）  
**Primary Dependencies**: `modules/dispense-packaging` `DispensePlanningFacade` / `PreviewSnapshotService` / `TriggerPlanner`；`modules/workflow` `PlanningUseCase` / `DispensingWorkflowUseCase` / `DispensingPlannerService`；`apps/runtime-gateway` TCP `dxf.preview.snapshot` dispatcher；`apps/hmi-app` preview session / `main_window.py`；`shared/contracts/application/commands/dxf.command-set.json`；GoogleTest / CTest；`pytest`  
**Storage**: Git 跟踪的仓库源码、spec 产物、契约文档、测试与基线资产；无数据库  
**Testing**: 根级 `.\build.ps1 -Profile Local -Suite all`、`.\test.ps1 -Profile CI -Suite all`；定向 `ctest --test-dir .\build --output-on-failure -R "DispensePlanningFacade|CMPCoordinatedInterpolatorPrecision|DispensingWorkflowUseCase|PlanningUseCaseExportPort"`；`python -m pytest .\apps\hmi-app\tests\unit\test_protocol_preview_gate_contract.py -q`；`python -m pytest .\apps\hmi-app\tests\unit\test_main_window.py -q`；`python -m pytest .\apps\runtime-gateway\transport-gateway\tests\test_transport_gateway_compatibility.py -q`  
**Target Platform**: Windows 桌面 HMI + runtime gateway + workflow/packaging 规划链  
**Project Type**: 多模块 C++ 规划/协议栈 + Python 桌面 HMI 消费端  
**Performance Goals**: 相同输入图形与规划参数必须产出确定性的胶点数量与坐标；预览请求保持在现有单次 prepare/snapshot 交互路径内，不新增额外网关往返或二次全局采样链  
**Constraints**: 仅接受 `planned_glue_snapshot` + `glue_points`；预览与执行必须消费同一份权威出胶结果；禁止隐式 fallback 到 `motion_trajectory_points`、`trigger_distances_mm` 回推、固定间隔采样或历史结果；短线段例外按锚定后实际间距是否落出 `2.7-3.3 mm` 判定；旧格式结果明确失败且不自动转换；仅使用 canonical workspace roots  
**Scale/Scope**: 单一 feature slice，覆盖 `modules/dispense-packaging` owner logic、`modules/workflow` 编排与执行门禁、`apps/runtime-gateway` 预览协议守门、`apps/hmi-app` 权威来源展示与告警、`shared/contracts` 协议文档以及对应单元/契约回归

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] Branch name is compliant with `<type>/<scope>/<ticket>-<short-desc>`
- [x] Planned paths use canonical workspace roots; no new legacy default paths
- [x] Build/test validation approach is explicit via root entry points (`.\build.ps1`, `.\test.ps1`, `.\ci.ps1`) plus justified targeted checks (`ctest --test-dir .\build --output-on-failure -R "DispensePlanningFacade|CMPCoordinatedInterpolatorPrecision|DispensingWorkflowUseCase|PlanningUseCaseExportPort"`, `python -m pytest .\apps\hmi-app\tests\unit\test_protocol_preview_gate_contract.py -q`, `python -m pytest .\apps\hmi-app\tests\unit\test_main_window.py -q`, `python -m pytest .\apps\runtime-gateway\transport-gateway\tests\test_transport_gateway_compatibility.py -q`)
- [x] Compatibility/fallback behavior is explicit and time-bounded if legacy paths are involved（本特性将旧 preview fallback 与旧结果自动转换全部定义为失败边界，不新增隐藏兼容开关）

## Project Structure

### Documentation (this feature)

```text
D:\Projects\SiligenSuite\specs\fix\workflow\SPEC-001-glue-point-alignment\
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts\
└── tasks.md
```

### Source Code (repository root)

```text
D:\Projects\SiligenSuite\modules\dispense-packaging\
├── application\include\application\services\dispensing\DispensePlanningFacade.h
├── application\services\dispensing\DispensePlanningFacade.cpp
└── tests\unit\application\services\dispensing\DispensePlanningFacadeTest.cpp

D:\Projects\SiligenSuite\modules\workflow\
├── application\usecases\dispensing\PlanningUseCase.cpp
├── application\usecases\dispensing\PlanningUseCase.h
├── application\usecases\dispensing\DispensingWorkflowUseCase.cpp
├── application\services\dispensing\PlanningPreviewAssemblyService.cpp
├── application\services\dispensing\WorkflowPreviewSnapshotService.cpp
├── domain\domain\dispensing\planning\domain-services\DispensingPlannerService.cpp
└── tests\process-runtime-core\unit\

D:\Projects\SiligenSuite\apps\runtime-gateway\transport-gateway\
├── src\tcp\TcpCommandDispatcher.cpp
└── tests\test_transport_gateway_compatibility.py

D:\Projects\SiligenSuite\apps\hmi-app\
├── src\hmi_client\ui\main_window.py
└── tests\unit\

D:\Projects\SiligenSuite\shared\contracts\application\
├── commands\dxf.command-set.json
└── mappings\protocol-mapping.md
```

**Structure Decision**: 维持现有 canonical 多模块分层，不新增新的顶层项目。权威胶点生成与执行一致性约束落在 `modules/dispense-packaging` 和 `modules/workflow` 的 planning owner chain；workflow app 层继续负责编排与执行门禁；runtime-gateway 继续负责 `planned_glue_snapshot` 协议守门；HMI 继续作为 `glue_points` 主预览消费者；正式外部协议仍以 `shared/contracts/application` 为准。

## Post-Design Constitution Re-check

- [x] 设计仍只使用 `apps/`、`modules/`、`shared/`、`tests/`、`docs/`、`scripts/`、`config/`、`data/`、`deploy/` canonical roots
- [x] 权威 owner surface 继续留在既有 planning / packaging / workflow 模块中，不把 HMI 或临时工具重新拉成默认 owner
- [x] 验证路径保持以根级入口为主，并辅以明确的定向 C++ / Python 契约与 UI 回归
- [x] 兼容行为被收敛为显式失败边界：旧结果不自动转换、旧 fallback 不隐藏保留、旧 `runtime_snapshot` 语义不再作为主预览依据

## Complexity Tracking

当前无宪章豁免项；本节保持空白。
