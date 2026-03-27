# US1 Owner Closure Evidence

## File-Level Changes

1. 新增 `modules/motion-planning/domain/motion/domain-services/MotionPlanner.h/.cpp`
2. `modules/motion-planning/domain/motion/CMakeLists.txt` 改为编译本模块 `MotionPlanner.cpp`
3. `modules/motion-planning/application/services/motion_planning/MotionPlanningFacade.cpp` 改为 include `domain/motion/domain-services/MotionPlanner.h`
4. `modules/process-path/domain/trajectory/domain-services/MotionPlanner.h` 改为兼容别名头
5. `modules/process-path/domain/trajectory/domain-services/MotionPlanner.cpp` 改为兼容占位翻译单元
6. `modules/process-path/domain/trajectory/CMakeLists.txt` 移除对 motion-planning include 根的反向暴露
7. 新增 `modules/motion-planning/tests/unit/domain/motion/MotionPlannerOwnerPathTest.cpp`

## Guardrail Changes

1. `scripts/validation/assert-module-boundary-bridges.ps1` 增加：
   - `motion-planning-still-compiles-process-path-motion-planner`
   - `motion-planning-facade-still-uses-legacy-motion-planner-include`

## Verification Commands

1. `pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/validation/assert-module-boundary-bridges.ps1 -WorkspaceRoot . -ReportDir tests/reports/module-boundary-bridges`
2. `pwsh -NoProfile -ExecutionPolicy Bypass -File test.ps1 -Profile CI -Suite contracts -ReportDir tests/reports/m7-owner-boundary-repair -FailOnKnownFailure`
