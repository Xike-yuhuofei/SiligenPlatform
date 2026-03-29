---
description: "Task list for HMI 胶点预览锚定分布一致性"
---

# Tasks: HMI 胶点预览锚定分布一致性

**Input**: 设计文档来自 `D:\Projects\SiligenSuite\specs\fix\workflow\SPEC-001-glue-point-alignment\`  
**Prerequisites**: `plan.md`、`spec.md`、`research.md`、`data-model.md`、`contracts\`、`quickstart.md`

**Tests**: 本特性已在 `spec.md`、`plan.md` 和 `quickstart.md` 中冻结了验收场景与定向回归命令，因此每个用户故事都包含先行测试任务。  
**Organization**: 任务按用户故事分组，确保 `US1`、`US2`、`US3` 可以按角点共点、均匀分布、authority 收口三步独立实施和验证。

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 可并行执行（不同文件、无未完成前置依赖）
- **[Story]**: 任务对应的用户故事（`US1`、`US2`、`US3`）
- 每条任务都包含可直接落地的精确文件路径

## Phase 1: Setup (Shared Fixtures and Baselines)

**Purpose**: 建立跨故事复用的几何夹具、协议样例和回归入口

- [X] T001 在 `D:\Projects\SiligenSuite\modules\dispense-packaging\tests\unit\application\services\dispensing\DispensePlanningFacadeTest.cpp` 建立方框、对角线、共享顶点、短线段和多轮廓几何夹具
- [X] T002 [P] 在 `D:\Projects\SiligenSuite\apps\runtime-gateway\transport-gateway\tests\test_transport_gateway_compatibility.py` 和 `D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\test_protocol_preview_gate_contract.py` 建立 `planned_glue_snapshot + glue_points` 权威快照基线样例

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: 固定所有用户故事共用的 authority 数据流、预览状态和导出约束

**CRITICAL**: 本阶段完成前，不得开始任何用户故事实现

- [X] T003 在 `D:\Projects\SiligenSuite\modules\dispense-packaging\application\include\application\services\dispensing\DispensePlanningFacade.h` 和 `D:\Projects\SiligenSuite\modules\dispense-packaging\application\services\dispensing\DispensePlanningFacade.cpp` 引入按几何线段组织的 authority trigger、spacing validation group 和 preview glue segment 辅助结构
- [X] T004 [P] 在 `D:\Projects\SiligenSuite\modules\workflow\domain\include\domain\dispensing\planning\domain-services\DispensingPlannerService.h` 和 `D:\Projects\SiligenSuite\modules\workflow\domain\domain\dispensing\planning\domain-services\DispensingPlannerService.cpp` 固定“显式 `enable_position_trigger` 插补点列是唯一 preview authority”的领域服务契约
- [X] T005 [P] 在 `D:\Projects\SiligenSuite\modules\workflow\application\usecases\dispensing\DispensingWorkflowUseCase.cpp` 和 `D:\Projects\SiligenSuite\modules\workflow\application\services\dispensing\WorkflowPreviewSnapshotService.cpp` 对齐 preview snapshot hash、plan fingerprint、preview state 与 execution gate 的共用 authority 指纹

**Checkpoint**: authority owner chain、preview snapshot 状态和执行门禁依赖已经固定，用户故事可以开始实施

---

## Phase 3: User Story 1 - 角点与共享顶点严格共点 (Priority: P1)

**Goal**: 让计划胶点预览在每个端点、角点和共享顶点上都只保留一个与目标几何重合的胶点

**Independent Test**: 使用包含方框、对角线和共享顶点的参考图形生成预览，确认每个目标顶点都被单一胶点准确命中，且没有可分辨的近邻点簇

### Tests for User Story 1

> 先写这些回归，并确认它们在实现前失败

- [X] T006 [P] [US1] 在 `D:\Projects\SiligenSuite\modules\dispense-packaging\tests\unit\application\services\dispensing\DispensePlanningFacadeTest.cpp` 添加角点、闭合轮廓顶点和共享顶点单点命中回归
- [X] T007 [P] [US1] 在 `D:\Projects\SiligenSuite\modules\workflow\tests\process-runtime-core\unit\dispensing\PlanningUseCaseExportPortTest.cpp` 添加共享顶点导出稳定性和重复预览坐标不漂移回归

### Implementation for User Story 1

- [X] T008 [US1] 在 `D:\Projects\SiligenSuite\modules\dispense-packaging\application\services\dispensing\DispensePlanningFacade.cpp` 以每条 `dispense_on` 几何线段的首尾双锚定替换当前 preview glue point 重建逻辑，并对共享顶点做单坐标去重
- [X] T009 [US1] 在 `D:\Projects\SiligenSuite\modules\workflow\domain\domain\dispensing\planning\domain-services\DispensingPlannerService.cpp` 重写 `BuildPreviewPoints(...)` 使其仅从显式位置触发生成共点 preview 点，并保持极小舍入误差内的坐标稳定

**Checkpoint**: US1 完成后，角点和共享顶点应能作为执行前确认依据被稳定命中

---

## Phase 4: User Story 2 - 每条线段按 3.0 mm 附近均匀分布并正确处理短线段例外 (Priority: P2)

**Goal**: 让每条几何线段在保留首尾锚点的前提下按目标 3.0 mm 附近均匀分布，并把短线段例外与跨轮廓 spacing 复核分开处理

**Independent Test**: 选取边长不是 3.0 mm 整数倍的矩形、折线和短线段样例，确认首尾锚点保留、段内均匀分布、非例外段落在 `2.7-3.3 mm`、例外段被单独标记

### Tests for User Story 2

> 先写这些回归，并确认它们在实现前失败

- [X] T010 [P] [US2] 在 `D:\Projects\SiligenSuite\modules\dispense-packaging\tests\unit\application\services\dispensing\DispensePlanningFacadeTest.cpp` 添加非整倍数边长均分、短线段例外和跨轮廓 spacing 污染防回归
- [X] T011 [P] [US2] 在 `D:\Projects\SiligenSuite\modules\workflow\tests\process-runtime-core\unit\domain\motion\CMPCoordinatedInterpolatorPrecisionTest.cpp` 添加按线段锚定后的实际点距窗口和顺序稳定性回归

### Implementation for User Story 2

- [X] T012 [US2] 在 `D:\Projects\SiligenSuite\modules\dispense-packaging\application\services\dispensing\DispensePlanningFacade.cpp` 实现 `n = max(1, round(L / 3.0))` 的段内均分、短线段例外判定和按真实几何分组的 `ValidateGlueSpacing(...)`
- [X] T013 [US2] 在 `D:\Projects\SiligenSuite\modules\workflow\domain\domain\dispensing\planning\domain-services\DispensingPlannerService.cpp` 保持锚定后 authority trigger 的路径顺序、连续出胶区间和 `derived_trigger_distances_mm` 兼容导出不变，并显式拒绝零长度段、重复点和无效段

**Checkpoint**: US2 完成后，常规段应满足 spacing 窗口，短线段应以例外方式保留首尾锚点并被清晰区分

---

## Phase 5: User Story 3 - 仅接受权威显式触发结果并阻止非权威预览继续执行 (Priority: P3)

**Goal**: 让 preview 与 execution 只消费同一份权威显式触发结果，并在 authority 缺失、旧结果或 fallback 场景下明确失败和阻止启动

**Independent Test**: 人为构造 authority 缺失但旧 fallback 仍可用的场景，确认 preview 请求失败、gateway/HMI 拒绝非权威快照、`StartJob` 被阻止

### Tests for User Story 3

> 先写这些回归，并确认它们在实现前失败

- [X] T014 [P] [US3] 在 `D:\Projects\SiligenSuite\modules\workflow\tests\process-runtime-core\unit\dispensing\DispensingWorkflowUseCaseTest.cpp` 和 `D:\Projects\SiligenSuite\modules\workflow\tests\process-runtime-core\unit\dispensing\PlanningUseCaseExportPortTest.cpp` 添加 authority 缺失、旧格式结果拒绝和 preview gate 阻止启动回归
- [X] T015 [P] [US3] 在 `D:\Projects\SiligenSuite\apps\runtime-gateway\transport-gateway\tests\test_transport_gateway_compatibility.py`、`D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\test_protocol_preview_gate_contract.py` 和 `D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\test_main_window.py` 添加非 `planned_glue_snapshot`、非 `glue_points`、空 authority 载荷和 authority mismatch 回归

### Implementation for User Story 3

- [X] T016 [US3] 在 `D:\Projects\SiligenSuite\modules\dispense-packaging\application\services\dispensing\DispensePlanningFacade.cpp` 移除 preview 对 `trigger_distances_mm`、`motion_trajectory_points` 和固定间隔采样的 fallback，只允许显式 `enable_position_trigger` 插补点列生成 `glue_points`
- [X] T017 [US3] 在 `D:\Projects\SiligenSuite\modules\workflow\application\usecases\dispensing\DispensingWorkflowUseCase.cpp` 和 `D:\Projects\SiligenSuite\modules\workflow\application\services\dispensing\WorkflowPreviewSnapshotService.cpp` 将 authority 缺失、来源不合规、旧格式结果和 spacing 校验失败统一升级为 preview `FAILED` 并阻止 `StartJob`
- [X] T018 [US3] 在 `D:\Projects\SiligenSuite\apps\runtime-gateway\transport-gateway\src\tcp\TcpCommandDispatcher.cpp` 和 `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\ui\main_window.py` 明确拦截并提示非权威 preview source、preview kind 和空 `glue_points` 快照

**Checkpoint**: US3 完成后，旧 fallback 和历史结果都不能再被误认为当前可执行预览

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: 对齐正式协议映射并执行定向与全量验证

- [X] T019 [P] 在 `D:\Projects\SiligenSuite\shared\contracts\application\mappings\protocol-mapping.md` 对齐 `planned_glue_snapshot + glue_points` 的 authority 语义、失败边界和 execution gate 依赖
- [X] T020 [P] 运行 `D:\Projects\SiligenSuite\specs\fix\workflow\SPEC-001-glue-point-alignment\quickstart.md` 中的定向回归命令：`cmake --build .\build --config Debug --target siligen_dispense_packaging_unit_tests siligen_motion_planning_unit_tests siligen_unit_tests -- /m:1`、`& .\build\bin\Debug\siligen_dispense_packaging_unit_tests.exe --gtest_filter="DispensePlanningFacadeTest.*"`、`& .\build\bin\Debug\siligen_motion_planning_unit_tests.exe --gtest_filter="CMPCoordinatedInterpolatorPrecisionTest.*"`、`& .\build\bin\Debug\siligen_unit_tests.exe --gtest_filter="DispensingWorkflowUseCaseTest.*:PlanningUseCaseExportPortTest.*"`、`python -m pytest .\apps\hmi-app\tests\unit\test_protocol_preview_gate_contract.py -q`、`python -m pytest .\apps\hmi-app\tests\unit\test_main_window.py -q`、`python -m pytest .\apps\runtime-gateway\transport-gateway\tests\test_transport_gateway_compatibility.py -q`
- [X] T021 运行 `D:\Projects\SiligenSuite\specs\fix\workflow\SPEC-001-glue-point-alignment\quickstart.md` 中的根级验证命令：`powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile Local -Suite all` 和 `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite all -ReportDir "$env:TEMP\siligen-spec-001-root-validation"`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: 无依赖，可立即开始
- **Phase 2 (Foundational)**: 依赖 Phase 1，阻塞全部用户故事
- **Phase 3 (US1)**: 依赖 Phase 2，是建议 MVP
- **Phase 4 (US2)**: 依赖 US1 的锚定共点输出稳定后再推进
- **Phase 5 (US3)**: 依赖 US1 和 US2 的 authority 结果与 spacing 校验口径稳定后再收口失败门禁
- **Phase 6 (Polish)**: 依赖全部目标用户故事完成

### User Story Dependencies

- **US1 (P1)**: 无故事级前置依赖，是本特性的 MVP
- **US2 (P2)**: 依赖 US1 已经产出稳定的按线段锚定 authority 点集
- **US3 (P3)**: 依赖 US1/US2 已经完成 authority 收口和 spacing 规则，否则无法正确硬失败

### Within Each User Story

- 测试任务必须先写并先失败，再进入对应实现任务
- `D:\Projects\SiligenSuite\modules\dispense-packaging\application\services\dispensing\DispensePlanningFacade.cpp` 是 US1/US2/US3 的共享 owner 文件，应按故事优先级顺序串行推进
- `D:\Projects\SiligenSuite\modules\workflow\domain\domain\dispensing\planning\domain-services\DispensingPlannerService.cpp` 是 US1/US2 的共享领域文件，应在同一 authority contract 下连续完成
- consumer 侧的 `TcpCommandDispatcher.cpp` 和 `main_window.py` 必须在 provider 侧 authority 语义固定后再收紧

### Parallel Opportunities

- Phase 1 的 `T002` 可与 `T001` 并行
- Phase 2 的 `T004`、`T005` 可并行
- US1 的 `T006`、`T007` 可并行
- US2 的 `T010`、`T011` 可并行
- US3 的 `T014`、`T015` 可并行
- Phase 6 的 `T019`、`T020` 可并行，`T021` 应在定向回归稳定后执行

---

## Parallel Example: User Story 1

```text
T006 + T007
```

## Parallel Example: User Story 2

```text
T010 + T011
```

## Parallel Example: User Story 3

```text
T014 + T015
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. 完成 Phase 1: Setup
2. 完成 Phase 2: Foundational
3. 完成 Phase 3: US1
4. 按 US1 的 Independent Test 验证角点和共享顶点是否严格共点
5. 仅在 US1 稳定后再继续后续故事

### Incremental Delivery

1. 先完成 Setup + Foundational，固定 authority chain 和 preview gate 状态
2. 交付 US1，解决角点、端点和共享顶点严格命中问题
3. 交付 US2，补齐均匀分布、短线段例外和 grouped spacing validation
4. 交付 US3，清除 fallback、收紧 authority source 并阻止非权威预览启动
5. 执行 Polish，完成协议映射和定向/全量验证

### Suggested MVP Scope

1. 建议 MVP 仅包含 `US1`
2. `US2` 和 `US3` 都复用 `US1` 建立的 authority 锚定输出
3. 若 `US1` 未稳定，不应并行开启后续 owner 文件改动

---

## Notes

- `[P]` 只表示可并行，不表示可以跳过前置 authority contract
- `planned_glue_snapshot + glue_points` 是整个任务清单默认的正式 preview 契约
- `trigger_distances_mm` 只允许作为兼容导出产物存在，不能再作为 preview authority
- 历史结果、旧格式结果和 fallback 轨迹都按失败边界处理
- 分支必须持续保持 `<type>/<scope>/<ticket>-<short-desc>` 合规
