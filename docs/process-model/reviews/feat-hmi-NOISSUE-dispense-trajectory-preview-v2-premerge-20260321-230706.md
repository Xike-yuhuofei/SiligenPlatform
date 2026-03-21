# Premerge Review Report

- branch: `feat/hmi/NOISSUE-dispense-trajectory-preview-v2`
- base: `main`
- baseline synced: `git fetch origin main` (done)
- scope: `packages/process-runtime-core` 本轮修复相关文件（执行/工作流/监控/阀门/冗余存储及对应测试）
- generated_at: `2026-03-21`

## Findings

### P1-1 无超时的在途任务等待可能导致析构卡死（阻断）
- files:
  - `packages/process-runtime-core/src/application/usecases/dispensing/DispensingExecutionUseCase.Async.cpp:165`
  - `packages/process-runtime-core/src/application/usecases/dispensing/DispensingExecutionUseCase.cpp:361`
- 风险:
  - `WaitForAllInflightTasks()` 是无限等待；若调度器任务提交成功但既未执行 runner、也未被成功取消，析构会永久阻塞。
  - 直接影响停机/重启链路，表现为进程退出卡死或看门狗重复拉起。
- 触发条件:
  - `SubmitTask` 成功后，调度器异常（队列阻塞、状态遗失、取消失败）导致 `inflight_tasks_` 无法归零。
- 建议修复:
  - 增加“可诊断的有界等待 + 二次状态判定”路径，至少输出 `failure_stage=destructor_wait_inflight` 与任务上下文。
  - 明确 `ITaskSchedulerPort` 的取消/状态一致性契约，并在适配器层提供可验证保障（否则业务层无法可靠收敛）。

### P1-2 `StartJob` 线程启动异常未收敛，会留下脏状态（阻断）
- files:
  - `packages/process-runtime-core/src/application/usecases/dispensing/DispensingWorkflowUseCase.cpp:391`
  - `packages/process-runtime-core/src/application/usecases/dispensing/DispensingWorkflowUseCase.cpp:410`
- 风险:
  - `std::thread` 构造抛异常时没有 `try/catch`，`Result` 语义被破坏（异常穿透）。
  - `jobs_` 与 `active_job_id_` 已提前写入，导致“看起来有活动作业但实际没有 worker”的假在线状态。
- 触发条件:
  - 线程资源不足或系统级线程创建失败。
- 建议修复:
  - 将线程创建放入 `try/catch`，失败时回滚 `jobs_` / `active_job_id_` 并返回 `THREAD_START_FAILED`。
  - 增加单测覆盖“线程启动失败回滚”。

### P2-1 冗余存储仅在“文件打开失败”时尝试 `.bak` 恢复，未覆盖“可打开但 JSON 损坏”路径
- files:
  - `packages/process-runtime-core/src/infrastructure/adapters/redundancy/JsonRedundancyRepositoryAdapter.cpp:56`
  - `packages/process-runtime-core/src/infrastructure/adapters/redundancy/JsonRedundancyRepositoryAdapter.cpp:81`
- 风险:
  - 主文件部分损坏时会直接返回 `JSON_PARSE_ERROR`，即使 `.bak` 有效也不会恢复，降低可恢复性。
- 触发条件:
  - 异常掉电/外部改写导致文件内容损坏但文件句柄可正常打开。
- 建议修复:
  - 在解析失败分支追加 `.bak` 读取与恢复逻辑；恢复失败时返回 stage 化错误（含主文件和备份文件上下文）。
  - 增加“主文件损坏 + 备份可用”的回归用例。

## Open Questions

1. `StopJob` 语义是否要求“同步确认终态”还是“仅转发取消请求成功即可返回”？当前行为偏向后者，外部轮询策略需明确契约。
2. `ITaskSchedulerPort::CancelTask` 在 `PENDING` 任务上的一致性保障是否可强约束（成功即不可执行）？如果不能，需要补偿机制。

## Residual Risks

- 当前修复已显著提升错误诊断与状态一致性，但上述两个 P1 仍会影响“关机收敛”和“作业生命周期可信度”。
- 测试仍偏 happy-path + 局部并发，极端时序（线程启动失败、调度器失步）缺少最小闭环验证。

## 建议下一步

1. 先修 P1-1 / P1-2，再进入合并窗口。
2. 修复后至少执行：
   - `siligen_unit_tests --gtest_filter=DispensingExecutionUseCaseProgressTest.*:DispensingWorkflowUseCaseTest.*`
   - `siligen_unit_tests --gtest_filter=JsonRedundancyRepositoryAdapterTest.*`
3. 补齐上述失败路径单测后再做最终 premerge gate。

