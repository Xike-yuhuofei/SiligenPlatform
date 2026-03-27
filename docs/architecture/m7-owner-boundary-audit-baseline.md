# M7 Owner Boundary Audit Baseline (ARCH-201)

## Baseline Date

- 2026-03-27

## Baseline Findings

1. `modules/motion-planning/domain/motion/CMakeLists.txt` 直接编译 `modules/process-path/.../MotionPlanner.cpp`
2. `MotionPlanningFacade.cpp` 直接引用 `domain/trajectory/domain-services/MotionPlanner.h`
3. `process-path/domain/trajectory/CMakeLists.txt` 向外暴露 `motion-planning` include 根

## Target State

1. `MotionPlanner` canonical 实现在 M7 owner 根
2. 旧路径仅保留兼容包装并标记退出条件
3. 边界门禁覆盖“旧 include / 旧编译路径”回退场景
