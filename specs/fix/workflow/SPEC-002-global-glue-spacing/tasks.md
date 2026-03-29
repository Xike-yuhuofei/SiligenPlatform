---
description: "Task list for 胶点全局均布与真值链一致性"
---

# Tasks: 胶点全局均布与真值链一致性

**Input**: 设计文档来自 `D:\Projects\SiligenSuite\specs\fix\workflow\SPEC-002-global-glue-spacing\`  
**Prerequisites**: `plan.md`、`spec.md`、`research.md`、`data-model.md`、`contracts\`、`quickstart.md`

**Tests**: 本特性在 `spec.md`、`plan.md` 和 `quickstart.md` 中已经冻结了独立验收标准和定向回归命令，因此每个用户故事都包含先行测试任务。  
**Organization**: 任务按用户故事分组，确保 `US1`、`US2`、`US3` 分别围绕全局均布、同源 authority、闭环/曲线可解释性独立实施和验证。

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 可并行执行（不同文件、无未完成前置依赖）
- **[Story]**: 任务对应的用户故事（`US1`、`US2`、`US3`）
- 每条任务都包含可直接落地的精确文件路径

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: 建立本特性复用的构建入口、夹具和回归样例

- [X] T001 在 `D:\Projects\SiligenSuite\modules\dispense-packaging\domain\dispensing\CMakeLists.txt`、`D:\Projects\SiligenSuite\modules\dispense-packaging\tests\CMakeLists.txt` 和 `D:\Projects\SiligenSuite\modules\workflow\tests\process-runtime-core\CMakeLists.txt` 注册 `AuthorityTriggerLayoutPlanner`、`PathArcLengthLocator`、`CurveFlatteningService` 及新增单元测试文件
- [X] T002 [P] 在 `D:\Projects\SiligenSuite\modules\dispense-packaging\tests\unit\application\services\dispensing\DispensePlanningFacadeTest.cpp` 和 `D:\Projects\SiligenSuite\modules\workflow\tests\process-runtime-core\unit\dispensing\PlanningUseCaseExportPortTest.cpp` 增加连续 span、闭环、曲线、无效几何的共享夹具与样例构造器

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: 固定所有用户故事共用的 authority layout 数据结构、状态载体和 preview 装配入口

**CRITICAL**: 本阶段完成前，不得开始任何用户故事实现

- [X] T003 在 `D:\Projects\SiligenSuite\modules\dispense-packaging\domain\dispensing\value-objects\AuthorityTriggerLayout.h` 定义 `AuthorityTriggerLayout`、`DispenseSpan`、`LayoutTriggerPoint`、`InterpolationTriggerBinding` 和 `SpacingValidationOutcome` 结构
- [X] T004 [P] 在 `D:\Projects\SiligenSuite\modules\dispense-packaging\domain\dispensing\planning\domain-services\AuthorityTriggerLayoutPlanner.h`、`D:\Projects\SiligenSuite\modules\dispense-packaging\domain\dispensing\planning\domain-services\AuthorityTriggerLayoutPlanner.cpp`、`D:\Projects\SiligenSuite\modules\dispense-packaging\domain\dispensing\planning\domain-services\PathArcLengthLocator.h`、`D:\Projects\SiligenSuite\modules\dispense-packaging\domain\dispensing\planning\domain-services\PathArcLengthLocator.cpp`、`D:\Projects\SiligenSuite\modules\dispense-packaging\domain\dispensing\planning\domain-services\CurveFlatteningService.h` 和 `D:\Projects\SiligenSuite\modules\dispense-packaging\domain\dispensing\planning\domain-services\CurveFlatteningService.cpp` 建立服务骨架与公开接口
- [X] T005 [P] 在 `D:\Projects\SiligenSuite\modules\dispense-packaging\application\include\application\services\dispensing\DispensePlanningFacade.h`、`D:\Projects\SiligenSuite\modules\workflow\application\include\application\usecases\dispensing\PlanningUseCase.h` 和 `D:\Projects\SiligenSuite\modules\workflow\application\include\application\usecases\dispensing\DispensingWorkflowUseCase.h` 增加 authority layout、binding readiness、validation classification 和 exception reason 载体字段
- [X] T006 在 `D:\Projects\SiligenSuite\modules\workflow\application\services\dispensing\PlanningPreviewAssemblyService.h`、`D:\Projects\SiligenSuite\modules\workflow\application\services\dispensing\PlanningPreviewAssemblyService.cpp`、`D:\Projects\SiligenSuite\modules\workflow\application\services\dispensing\WorkflowPreviewSnapshotService.h` 和 `D:\Projects\SiligenSuite\modules\workflow\application\services\dispensing\WorkflowPreviewSnapshotService.cpp` 对齐 authority layout 元数据输入口径，禁止预览装配层自行重建胶点真值

**Checkpoint**: authority layout 基础结构、状态载体和 preview 装配边界已固定，用户故事可开始推进

---

## Phase 3: User Story 1 - 连续路径均匀布点 (Priority: P1) 🎯 MVP

**Goal**: 让几何上等价但分段不同的连续路径产生相同数量、相同顺序且等价位置的胶点布局

**Independent Test**: 使用同一轮廓的“少量长段输入”和“多段细分输入”分别规划，确认 `glue_points` 数量、顺序和位置保持一致，且开放路径不再出现按段跳变

### Tests for User Story 1

> 先写这些回归，并确认它们在实现前失败

- [X] T007 [P] [US1] 在 `D:\Projects\SiligenSuite\modules\dispense-packaging\tests\unit\domain\dispensing\AuthorityTriggerLayoutPlannerTest.cpp` 添加分段等价性、开放 span 全局间距求解和重复布局稳定性回归
- [X] T008 [P] [US1] 在 `D:\Projects\SiligenSuite\modules\dispense-packaging\tests\unit\application\services\dispensing\DispensePlanningFacadeTest.cpp` 和 `D:\Projects\SiligenSuite\modules\workflow\tests\process-runtime-core\unit\dispensing\PlanningUseCaseExportPortTest.cpp` 添加 `glue_points` 不再依赖段内重建、重复规划坐标稳定的回归

### Implementation for User Story 1

- [X] T009 [US1] 在 `D:\Projects\SiligenSuite\modules\dispense-packaging\domain\dispensing\planning\domain-services\AuthorityTriggerLayoutPlanner.cpp` 实现连续 `dispense_on` span 提取、整数区间求解和全局 `LayoutTriggerPoint` 顺序生成
- [X] T010 [US1] 在 `D:\Projects\SiligenSuite\modules\dispense-packaging\domain\dispensing\planning\domain-services\PathArcLengthLocator.cpp` 实现 line/arc 路径的全局距离到局部几何位置映射
- [X] T011 [US1] 在 `D:\Projects\SiligenSuite\modules\dispense-packaging\application\services\dispensing\DispensePlanningFacade.cpp` 和 `D:\Projects\SiligenSuite\modules\dispense-packaging\domain\dispensing\planning\domain-services\DispensingPlannerService.cpp` 用 `AuthorityTriggerLayout` 替换当前按段 authority truth 和 `BuildPlannedTriggerPoints(...)` 预览重建热路径
- [X] T012 [US1] 在 `D:\Projects\SiligenSuite\modules\workflow\application\usecases\dispensing\PlanningUseCase.cpp` 改为直接导出 authority `glue_points`、layout 标识和稳定预览输入，不再暴露分段敏感的中间真值

**Checkpoint**: US1 完成后，相同真实路径的不同分段表达应收敛到同一胶点布局，可作为 MVP 交付

---

## Phase 4: User Story 2 - 预览、校验与执行同源 (Priority: P2)

**Goal**: 让 preview、spacing 校验和 execution launch 共用同一批 authority points，并在 authority 失配时明确阻止继续执行

**Independent Test**: 生成一组 authority 有效的计划和一组 authority 缺失或失配的计划，确认前者的 preview/start-job 共用同一 layout，后者明确失败

### Tests for User Story 2

> 先写这些回归，并确认它们在实现前失败

- [X] T013 [P] [US2] 在 `D:\Projects\SiligenSuite\modules\workflow\tests\process-runtime-core\unit\dispensing\DispensingWorkflowUseCaseTest.cpp` 添加 shared layout id、preview hash 稳定性、authority mismatch 和 start-job 阻断回归
- [X] T014 [P] [US2] 在 `D:\Projects\SiligenSuite\apps\runtime-gateway\transport-gateway\tests\test_transport_gateway_compatibility.py`、`D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\test_protocol_preview_gate_contract.py` 和 `D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\test_main_window.py` 添加非 authority preview、空 `glue_points` 和 preview/execution 失配回归

### Implementation for User Story 2

- [X] T015 [US2] 在 `D:\Projects\SiligenSuite\modules\dispense-packaging\application\services\dispensing\DispensePlanningFacade.cpp` 和 `D:\Projects\SiligenSuite\modules\dispense-packaging\domain\dispensing\planning\domain-services\DispensingPlannerService.cpp` 实现 `InterpolationTriggerBinding` 生成和 preview/execution 共用的 authority layout 状态输出
- [X] T016 [US2] 在 `D:\Projects\SiligenSuite\modules\workflow\application\usecases\dispensing\DispensingWorkflowUseCase.cpp`、`D:\Projects\SiligenSuite\modules\workflow\application\services\dispensing\WorkflowPreviewSnapshotService.cpp` 和 `D:\Projects\SiligenSuite\modules\workflow\application\services\dispensing\PlanningPreviewAssemblyService.cpp` 改为基于 shared authority layout gate preview/confirm/start-job，而不是基于轨迹回贴结果
- [X] T017 [US2] 在 `D:\Projects\SiligenSuite\apps\runtime-gateway\transport-gateway\src\tcp\TcpCommandDispatcher.cpp`、`D:\Projects\SiligenSuite\shared\contracts\application\commands\dxf.command-set.json`、`D:\Projects\SiligenSuite\shared\contracts\application\fixtures\responses\dxf.preview.snapshot.success.json` 和 `D:\Projects\SiligenSuite\shared\contracts\application\mappings\protocol-mapping.md` 固定 `planned_glue_snapshot + glue_points` 为唯一成功形态并写明 shared-authority 约束
- [X] T018 [US2] 在 `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\ui\main_window.py` 和 `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\tools\mock_server.py` 保持 authority `glue_points` 原样渲染，并将 authority-shared 失败态明确呈现为不可通过预览

**Checkpoint**: US2 完成后，preview、校验和执行准备必须明确共享同一 authority layout，authority 失配时禁止继续执行

---

## Phase 5: User Story 3 - 闭环与曲线场景可解释 (Priority: P3)

**Goal**: 让闭合轮廓、圆弧和平滑曲线上的胶点分布沿最终可见路径生成，并为例外和无效几何提供明确解释

**Independent Test**: 使用闭环、圆弧、平滑曲线、超短局部路径和无效几何样例，确认闭环不依赖固定起点，曲线胶点落在可见路径上，例外和失败原因可被读取

### Tests for User Story 3

> 先写这些回归，并确认它们在实现前失败

- [X] T019 [P] [US3] 在 `D:\Projects\SiligenSuite\modules\dispense-packaging\tests\unit\domain\dispensing\PathArcLengthLocatorTest.cpp` 和 `D:\Projects\SiligenSuite\modules\dispense-packaging\tests\unit\application\services\dispensing\DispensePlanningFacadeTest.cpp` 添加闭环相位、曲线可见路径定位和无效几何处理回归
- [X] T020 [P] [US3] 在 `D:\Projects\SiligenSuite\modules\workflow\tests\process-runtime-core\unit\dispensing\DispensingWorkflowUseCaseTest.cpp` 和 `D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\test_main_window.py` 添加 `pass_with_exception`、阻塞性失败原因和 HMI 解释性提示回归

### Implementation for User Story 3

- [X] T021 [US3] 在 `D:\Projects\SiligenSuite\modules\dispense-packaging\domain\dispensing\planning\domain-services\AuthorityTriggerLayoutPlanner.cpp` 实现闭合 span 候选相位评分、最小距离优先和确定性 tie-break
- [X] T022 [US3] 在 `D:\Projects\SiligenSuite\modules\dispense-packaging\domain\dispensing\planning\domain-services\CurveFlatteningService.cpp` 和 `D:\Projects\SiligenSuite\modules\dispense-packaging\domain\dispensing\planning\domain-services\PathArcLengthLocator.cpp` 实现 spline 误差受控扁平化与混合几何的可见路径定位
- [X] T023 [US3] 在 `D:\Projects\SiligenSuite\modules\dispense-packaging\application\services\dispensing\DispensePlanningFacade.cpp`、`D:\Projects\SiligenSuite\modules\workflow\application\usecases\dispensing\PlanningUseCase.cpp`、`D:\Projects\SiligenSuite\modules\workflow\application\usecases\dispensing\DispensingWorkflowUseCase.cpp` 和 `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\ui\main_window.py` 透传 `pass_with_exception` / `fail` 分类、例外原因和无效几何失败信息
- [X] T024 [US3] 在 `D:\Projects\SiligenSuite\modules\dispense-packaging\application\services\dispensing\PreviewSnapshotService.cpp` 和 `D:\Projects\SiligenSuite\modules\dispense-packaging\tests\unit\application\services\dispensing\PreviewSnapshotServiceTest.cpp` 明确 `runtime_snapshot` 仅为运行时诊断快照，禁止其重新进入 authority preview truth 流

**Checkpoint**: US3 完成后，闭环与曲线场景应具备稳定、可解释的布局结果，且例外与失败边界可被准确识别

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: 执行回归并完成跨故事收尾

- [X] T025 [P] 运行 `D:\Projects\SiligenSuite\specs\fix\workflow\SPEC-002-global-glue-spacing\quickstart.md` 中的定向回归命令：`cmake --build .\build --config Debug --target siligen_dispense_packaging_unit_tests siligen_unit_tests -- /m:1`、`& .\build\bin\Debug\siligen_dispense_packaging_unit_tests.exe --gtest_filter="DispensePlanningFacadeTest.*:TriggerPlannerTest.*:AuthorityTriggerLayoutPlannerTest.*:PathArcLengthLocatorTest.*"`、`& .\build\bin\Debug\siligen_unit_tests.exe --gtest_filter="DispensingWorkflowUseCaseTest.*:PlanningUseCaseExportPortTest.*"`、`python -m pytest .\apps\hmi-app\tests\unit\test_protocol_preview_gate_contract.py -q`、`python -m pytest .\apps\hmi-app\tests\unit\test_main_window.py -q` 和 `python -m pytest .\apps\runtime-gateway\transport-gateway\tests\test_transport_gateway_compatibility.py -q`
- [X] T026 运行 `D:\Projects\SiligenSuite\specs\fix\workflow\SPEC-002-global-glue-spacing\quickstart.md` 中的根级验证命令 `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile Local -Suite all` 和 `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite all`，并记录任何残余失败与跟进项

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: 无依赖，可立即开始
- **Phase 2 (Foundational)**: 依赖 Phase 1，阻塞全部用户故事
- **Phase 3 (US1)**: 依赖 Phase 2，是建议 MVP
- **Phase 4 (US2)**: 依赖 US1 已产出稳定的 authority layout 和 deterministic `glue_points`
- **Phase 5 (US3)**: 依赖 US1 的布局真值稳定，以及 US2 的 preview/execution 同源状态载体完成
- **Phase 6 (Polish)**: 依赖全部目标用户故事完成

### User Story Dependencies

- **US1 (P1)**: 无故事级前置依赖，是本特性的 MVP
- **US2 (P2)**: 依赖 US1 已固定 authority layout 作为唯一胶点真值
- **US3 (P3)**: 依赖 US1 的 span 布局能力和 US2 的 authority state / gate 口径

### Within Each User Story

- 测试任务必须先写并先失败，再进入对应实现任务
- `D:\Projects\SiligenSuite\modules\dispense-packaging\application\services\dispensing\DispensePlanningFacade.cpp` 是 US1、US2、US3 的共享 owner 文件，应按故事优先级顺序推进
- `D:\Projects\SiligenSuite\modules\workflow\application\usecases\dispensing\DispensingWorkflowUseCase.cpp` 是 US2、US3 的共享门禁文件，应先完成同源 gate，再补异常解释
- gateway/HMI consumer 侧的 `TcpCommandDispatcher.cpp` 和 `main_window.py` 必须在 provider 侧 authority 规则固定后再收紧

### Parallel Opportunities

- Phase 1 的 `T002` 可与 `T001` 并行
- Phase 2 的 `T004`、`T005` 可并行
- US1 的 `T007`、`T008` 可并行
- US2 的 `T013`、`T014` 可并行
- US3 的 `T019`、`T020` 可并行

---

## Parallel Example: User Story 1

```text
T007 + T008
```

## Parallel Example: User Story 2

```text
T013 + T014
```

## Parallel Example: User Story 3

```text
T019 + T020
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. 完成 Phase 1: Setup
2. 完成 Phase 2: Foundational
3. 完成 Phase 3: US1
4. 按 US1 的 Independent Test 验证连续路径全局均布是否已经摆脱分段敏感性
5. 仅在 US1 稳定后再继续后续故事

### Incremental Delivery

1. 先完成 Setup + Foundational，固定 authority layout、状态载体和 preview 装配边界
2. 交付 US1，解决全局均布与 deterministic `glue_points`
3. 交付 US2，完成 preview / 校验 / 执行同源 gate
4. 交付 US3，补齐闭环相位、曲线可见路径与异常解释
5. 执行 Polish，完成定向与根级验证

### Suggested MVP Scope

1. 建议 MVP 仅包含 `US1`
2. `US2` 和 `US3` 都复用 `US1` 建立的 authority layout 真值
3. 若 `US1` 未稳定，不应并行开启后续共享 owner 文件改动

---

## Notes

- `[P]` 只表示可并行，不表示可以跳过 authority contract 前置条件
- `planned_glue_snapshot + glue_points` 是整个任务清单默认的正式 preview 契约
- `runtime_snapshot` 只允许保留为运行时诊断快照，不得重新成为 authority preview truth
- 每条任务都已包含明确文件路径，便于直接进入 `/speckit.implement` 或人工拆分执行
