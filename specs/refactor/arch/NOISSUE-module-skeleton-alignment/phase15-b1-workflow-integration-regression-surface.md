# Phase 15: B1 workflow integration/regression surface

更新时间：`2026-03-25`

## 1. 目标

将 `modules/workflow/tests/integration` 与 `modules/workflow/tests/regression` 从 `.gitkeep` 占位目录提升为正式 canonical 测试承载面，并在不改 `workflow` owner 实现与 public surface 的前提下，完成可见的 target 登记。

## 2. 已完成变更

### 2.1 tests root 注册面扩展

- `modules/workflow/tests/CMakeLists.txt`
  - 保留 `process-runtime-core` canonical tests 入口。
  - 新增 `integration` 与 `regression` 子目录注册。
  - 抽取共享 include roots，供 workflow 新测试承载面复用。

### 2.2 integration 正式承载面落位

- `modules/workflow/tests/integration/CMakeLists.txt`
  - 新增 target：`workflow_integration_motion_runtime_assembly_smoke`
  - 复用 `tests/process-runtime-core/unit/motion/MotionRuntimeAssemblyFactoryTest.cpp`
  - 使用 canonical include/link roots，并沿用 runtime dependency staging / test environment 注册机制。
- `modules/workflow/tests/integration/README.md`
  - 固化 integration 面的目标、命名和后续扩展约束。

### 2.3 regression 正式承载面落位

- `modules/workflow/tests/regression/CMakeLists.txt`
  - 新增 target：`workflow_regression_deterministic_path_execution_smoke`
  - 复用 `tests/process-runtime-core/unit/motion/DeterministicPathExecutionUseCaseTest.cpp`
  - 作为最小路径执行回归哨兵登记到 canonical tests root。
- `modules/workflow/tests/regression/README.md`
  - 固化 regression 面的职责、命名和后续扩展约束。

## 3. 新增 target

- `workflow_integration_motion_runtime_assembly_smoke`
- `workflow_regression_deterministic_path_execution_smoke`
- `workflow_integration_tests`（聚合 target）
- `workflow_regression_tests`（聚合 target）

## 4. 当前仍缺的 workflow 集成场景

- workflow 初始化 facade 级集成测试
  - 当前只有 `process-runtime-core` 层面的 `InitializeSystemUseCase` 单测资产，尚未形成独立 integration target。
- workflow 装配到 runtime-execution/device adapters 的跨模块集成测试
  - 该类场景依赖更完整的宿主/设备装配面，不适合在本任务里仅靠测试登记面补齐。
- workflow assembly 失败路径与配置异常回归
  - 目前只登记了成功路径 smoke；错误路径矩阵需要后续任务补充更细颗粒度测试源。
- 路径执行与配方/冗余/解析 adapter 的组合回归
  - 现阶段 regression 仅覆盖确定性路径执行最小链路，尚未覆盖更宽的 adapter 组合面。

## 5. 说明

- 本阶段刻意不修改 `modules/workflow/CMakeLists.txt`、`domain/include`、`application/include`、`adapters/include`。
- integration/regression 新入口优先复用已稳定的 canonical 测试源文件，先把承载面、命名和注册面固定下来，再由后续任务继续扩容真实场景。
