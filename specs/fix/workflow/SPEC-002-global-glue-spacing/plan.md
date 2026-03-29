# Implementation Plan: 胶点全局均布与真值链一致性

**Branch**: `fix/workflow/SPEC-002-global-glue-spacing` | **Date**: 2026-03-29 | **Spec**: `D:\Projects\SiligenSuite\specs\fix\workflow\SPEC-002-global-glue-spacing\spec.md`  
**Input**: Feature specification from `D:\Projects\SiligenSuite\specs\fix\workflow\SPEC-002-global-glue-spacing\spec.md`

## Summary

将当前 `DispensePlanningFacade.cpp` / `DispensingPlannerService.cpp` 中“按段独立布点 + 插补轨迹回贴 + 预览二次重建”的链路，重构为“连续 dispense span 的权威胶点布局”。设计重点是：在 `modules/dispense-packaging` 的 domain planning 层新增全局布局服务，负责开放/闭合路径的统一距离求解、闭环相位选择和曲线真实路径定位；`DispensePlanningFacade`、`PlanningUseCase` 与 `DispensingWorkflowUseCase` 只消费同一批 authority points 来生成执行绑定、预览 `glue_points` 和门禁结论；对外仍保持 `dxf.preview.snapshot` 的 `planned_glue_snapshot + glue_points` 契约，不再把 `runtime_snapshot`、`motion_trajectory_points` 或显示专用重采样结果当作成功来源。

## Technical Context

**Language/Version**: C++17 / CMake 3.20+（`modules/dispense-packaging`、`modules/workflow`、`apps/runtime-gateway`）、Python 3.11（`apps/hmi-app` 测试与 mock 工具）、PowerShell 7（根级 `build.ps1` / `test.ps1` / `ci.ps1` 与 spec-kit 脚本）  
**Primary Dependencies**: `modules/dispense-packaging` `DispensePlanningFacade`、`DispensingPlannerService`、`TriggerPlanner`、`PreviewSnapshotService`、`TrajectoryTriggerUtils`；计划新增 `AuthorityTriggerLayoutPlanner`、`PathArcLengthLocator`、`CurveFlatteningService`、`AuthorityTriggerLayout`；`modules/workflow` `PlanningUseCase`、`DispensingWorkflowUseCase`、`WorkflowPreviewSnapshotService`；`apps/runtime-gateway` `TcpCommandDispatcher`；`apps/hmi-app` `main_window.py`；`shared/contracts/application/commands/dxf.command-set.json` 与 fixtures；GoogleTest / CTest；`pytest`  
**Storage**: Git 跟踪的仓库源码、spec 产物、协议契约、fixture 与测试资产；无数据库  
**Testing**: 根级 `.\build.ps1 -Profile Local -Suite all`、`.\test.ps1 -Profile CI -Suite all`；定向 C++ 回归 `siligen_dispense_packaging_unit_tests`、`siligen_unit_tests`、`TriggerPlannerTest` / 新增 authority layout 测试；Python 回归 `apps/hmi-app/tests/unit/test_protocol_preview_gate_contract.py`、`apps/hmi-app/tests/unit/test_main_window.py`、`apps/runtime-gateway/transport-gateway/tests/test_transport_gateway_compatibility.py`  
**Target Platform**: Windows 桌面 HMI + runtime gateway + workflow/packaging 规划链  
**Project Type**: 多模块 C++ 规划/协议栈 + Python 桌面 HMI 消费端  
**Performance Goals**: 相同有效几何路径与相同规划参数必须产出确定性的胶点数量、顺序与坐标；闭环相位搜索必须是有界且可重复的；预览生成继续停留在当前 prepare/snapshot 交互链内，不新增额外 gateway 往返  
**Constraints**: 对外 `dxf.preview.snapshot` schema 保持稳定；预览、校验与执行准备必须共享同一批 authority points；几何等价但分段方式不同的输入必须给出等价布局；闭合路径在无业务强锚点时不得依赖固定起点；曲线布局必须沿最终可见路径而不是控制骨架；允许显式、可解释的间距例外，但不允许隐式 fallback 到 `runtime_snapshot`、`motion_trajectory_points` 或显示专用重采样真值  
**Scale/Scope**: 单一 feature slice，覆盖 `modules/dispense-packaging` 的布局 owner logic、`modules/workflow` 的 preview/execution gate、`apps/runtime-gateway` 的 transport 守门、`apps/hmi-app` 的主预览语义与错误提示、`shared/contracts/application` 的正式契约与 fixture

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] Branch name is compliant with `<type>/<scope>/<ticket>-<short-desc>`
- [x] Planned paths use canonical workspace roots; no new legacy default paths
- [x] Build/test validation approach is explicit via root entry points (`.\build.ps1`, `.\test.ps1`, `.\ci.ps1`) plus justified targeted checks (`siligen_dispense_packaging_unit_tests`, `siligen_unit_tests`, gateway/HMI pytest suites)
- [x] Compatibility/fallback behavior is explicit and time-bounded if legacy paths are involved（现有 `dxf.preview.snapshot` 缺少 `plan_id` 的 transport 兼容分支不扩展 owner 语义；本特性不再引入新的 preview truth fallback，也不把 `runtime_snapshot` 重新拉回成功路径）

## Project Structure

### Documentation (this feature)

```text
D:\Projects\SiligenSuite\specs\fix\workflow\SPEC-002-global-glue-spacing\
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
├── application\services\dispensing\DispensePlanningFacade.cpp
├── application\include\application\services\dispensing\DispensePlanningFacade.h
├── application\services\dispensing\PreviewSnapshotService.cpp
├── domain\dispensing\planning\domain-services\DispensingPlannerService.cpp
├── domain\dispensing\planning\domain-services\AuthorityTriggerLayoutPlanner.h      # new
├── domain\dispensing\planning\domain-services\AuthorityTriggerLayoutPlanner.cpp     # new
├── domain\dispensing\planning\domain-services\PathArcLengthLocator.h                # new
├── domain\dispensing\planning\domain-services\PathArcLengthLocator.cpp              # new
├── domain\dispensing\planning\domain-services\CurveFlatteningService.h              # new
├── domain\dispensing\planning\domain-services\CurveFlatteningService.cpp             # new
├── domain\dispensing\value-objects\AuthorityTriggerLayout.h                         # new
├── tests\unit\application\services\dispensing\DispensePlanningFacadeTest.cpp
└── tests\unit\domain\dispensing\
   ├── TriggerPlannerTest.cpp
   ├── AuthorityTriggerLayoutPlannerTest.cpp                                          # new
   └── PathArcLengthLocatorTest.cpp                                                   # new

D:\Projects\SiligenSuite\modules\workflow\
├── application\usecases\dispensing\PlanningUseCase.cpp
├── application\usecases\dispensing\DispensingWorkflowUseCase.cpp
├── application\services\dispensing\PlanningPreviewAssemblyService.cpp
├── application\services\dispensing\WorkflowPreviewSnapshotService.cpp
└── tests\process-runtime-core\unit\dispensing\
   ├── DispensingWorkflowUseCaseTest.cpp
   └── PlanningUseCaseExportPortTest.cpp

D:\Projects\SiligenSuite\apps\runtime-gateway\transport-gateway\
├── src\tcp\TcpCommandDispatcher.cpp
└── tests\test_transport_gateway_compatibility.py

D:\Projects\SiligenSuite\apps\hmi-app\
├── src\hmi_client\ui\main_window.py
└── tests\unit\
   ├── test_main_window.py
   └── test_protocol_preview_gate_contract.py

D:\Projects\SiligenSuite\shared\contracts\application\
├── commands\dxf.command-set.json
├── fixtures\requests\dxf.preview.snapshot.request.json
├── fixtures\responses\dxf.preview.snapshot.success.json
└── mappings\protocol-mapping.md
```

**Structure Decision**: 维持现有 canonical 多模块分层，不新增新的顶层 owner surface。几何与布局真值统一收敛到 `modules/dispense-packaging/domain/dispensing/planning/domain-services`；`application/services/dispensing` 和 `modules/workflow/application` 只负责编排、绑定和门禁；`apps/runtime-gateway` 与 `apps/hmi-app` 继续是正式协议 consumer；`shared/contracts/application` 继续是正式对外契约根。

## Post-Design Constitution Re-check

- [x] 设计仍只使用 `apps/`、`modules/`、`shared/`、`tests/`、`docs/`、`scripts/`、`config/`、`data/`、`deploy/` canonical roots
- [x] 胶点布局 owner 继续位于 `modules/dispense-packaging` / `modules/workflow` 规划链，不把 HMI、gateway 或 spec 目录变成默认 owner
- [x] 验证路径保持以根级脚本为主，并辅以明确的 C++ / Python 定向回归
- [x] 兼容行为被收敛为显式边界：对外 schema 不变；旧 `runtime_snapshot` 不是成功主预览；旧 transport shim 不扩展为新的 truth source

## Complexity Tracking

当前无宪章豁免项；本节保持空白。
