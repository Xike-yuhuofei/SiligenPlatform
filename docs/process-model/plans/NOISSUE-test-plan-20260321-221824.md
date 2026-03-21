# NOISSUE Test Plan - process-runtime-core 并发与状态一致性修复

- 日期：2026-03-21
- 分支：`feat/hmi/NOISSUE-dispense-trajectory-preview-v2`
- 关联文档：
  - `docs/process-model/plans/NOISSUE-scope-20260321-221824.md`
  - `docs/process-model/plans/NOISSUE-arch-plan-20260321-221824.md`

## 1. 关键路径（必须通过）

1. `DispensingExecutionUseCase`：`ExecuteAsync -> CancelTask -> GetTaskStatus` 全链路终态一致。
2. `DispensingExecutionUseCase`：scheduler 路径与本地线程路径在析构时均无悬空任务。
3. `DispensingWorkflowUseCase`：`StopJob` 在竞态下幂等，返回结果与真实终态一致。
4. `MotionMonitoringUseCase`：`Start/Stop/析构` 无 detach 残留线程，状态可观测一致。
5. `ValveCoordinationService`：安全检查缺失时默认拒绝，不可“假安全通过”。
6. `JsonRedundancyRepositoryAdapter`：写入失败不破坏旧数据；错误语义可判定。

## 2. 边界场景（必须覆盖）

1. runner 抛异常（含 `std::exception` 与未知异常）时，任务终态应为 `failed` 且带错误信息。
2. `CancelTask` 返回成功但执行未停的历史场景，修复后必须不可复现。
3. `Pause/Resume` 轮询期任务已终态时，返回值不得固定为 `MOTION_TIMEOUT`。
4. `CleanupExpiredTasks` 遇到 `RUNNING/PAUSED` 超长任务不得直接删除。
5. 监控回调内触发 `StopStatusUpdate` / 重配周期，不得触发 UAF 或死锁。
6. Redundancy `priority` 非法输入需返回 `INVALID_PARAMETER`。
7. Redundancy 文件替换中断场景，旧文件应可读、无空洞损坏。

## 3. 回归基线

1. `packages/process-runtime-core/tests/unit/dispensing/*` 现有通过率不下降。
2. `packages/process-runtime-core/tests/unit/motion/*` 现有通过率不下降。
3. `packages/process-runtime-core/tests/unit/redundancy/*` 现有通过率不下降。
4. 全量 `siligen_unit_tests` 必须全绿，不引入新 flaky。

## 4. 测试命令（根级）与预期结果

1. 构建单测目标  
命令：`cmake --build build-tests --target siligen_unit_tests --config Debug`  
预期：构建成功，无新增编译告警升级为错误。

2. 定向回归（并发与状态机）  
命令：`./build-tests/bin/Debug/siligen_unit_tests.exe --gtest_filter=DispensingExecutionUseCaseProgressTest.*:DispensingWorkflowUseCaseTest.*:MotionMonitoringUseCaseTest.*:RedundancyGovernanceUseCasesTest.*`  
预期：全部通过；新增边界用例覆盖 P0/P1 对应缺陷。

3. Redundancy 适配器定向  
命令：`./build-tests/bin/Debug/siligen_unit_tests.exe --gtest_filter=JsonRedundancyRepositoryAdapterTest.*`  
预期：原子替换与非法输入语义相关用例通过。

4. 全量回归  
命令：`./build-tests/bin/Debug/siligen_unit_tests.exe`  
预期：全量通过；无崩溃、无超时假失败、无状态机回写冲突。

## 5. 新增测试项（实施清单）

1. `DispensingExecutionUseCase`：scheduler 任务未结束时析构等待与安全退出。
2. `DispensingExecutionUseCase`：runner 异常映射 `FAILED` 及错误上下文断言。
3. `DispensingExecutionUseCase`：`Pause/Resume` 观测终态时的返回语义断言。
4. `DispensingExecutionUseCase`：`CleanupExpiredTasks` 不删除活跃任务。
5. `DispensingWorkflowUseCase`：`StopJob` 取消失败后的状态补查与幂等行为。
6. `MotionMonitoringUseCase`：禁止 detach 后的析构无残留线程断言。
7. `ValveCoordinationService`：安全前置未满足时 `Start*` 拒绝启动。
8. `JsonRedundancyRepositoryAdapter`：替换失败场景下旧数据保全验证。

## 6. 判定标准与缺陷分级

1. P0/P1 相关用例任一失败即阻断合并。
2. P2 可在同迭代修复；若延期必须附风险与回归补偿。
3. P3 可延期，但必须建跟踪项并确保不影响本次合并门槛。

