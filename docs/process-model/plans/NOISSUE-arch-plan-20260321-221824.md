# NOISSUE Arch Plan - process-runtime-core 并发/状态机/错误语义修复

- 日期：2026-03-21
- 分支：`feat/hmi/NOISSUE-dispense-trajectory-preview-v2`
- 输入基线：`docs/process-model/plans/NOISSUE-scope-20260321-221824.md`

## 1. 架构方案结论（冻结）

1. 以“生命周期安全优先”收敛执行链路：所有后台任务必须在对象析构前可观测、可停止、可等待。
2. 以“终态唯一提交”收敛状态机：任何 RUNNING/PAUSED 回写不得覆盖已提交终态。
3. 以“控制语义真实化”收敛接口：取消成功必须可证明执行已停，暂停/恢复失败必须返回真实失败原因而非统一超时。
4. 以“安全默认拒绝”收敛阀门安全逻辑：`CheckValveSafety` 未具备完整能力前禁止默认放行。
5. 以“失败可诊断”收敛错误处理：关键路径统一补齐 failure_stage / failure_code / context。

## 2. 受影响目录与文件类型

1. `packages/process-runtime-core/src/application/usecases/dispensing/*.h|*.cpp`
2. `packages/process-runtime-core/src/application/usecases/motion/monitoring/*.h|*.cpp`
3. `packages/process-runtime-core/src/application/usecases/redundancy/*.h|*.cpp`
4. `packages/process-runtime-core/src/infrastructure/adapters/redundancy/*.h|*.cpp`
5. `packages/process-runtime-core/src/domain/dispensing/domain-services/ValveCoordinationService.*`
6. `packages/process-runtime-core/tests/unit/**/*.cpp`

## 3. 数据流 / 调用流（修复后）

### 3.1 异步执行（scheduler/local）统一生命周期

```text
ExecuteAsync
  -> 创建 TaskExecutionContext(含 lifecycle token)
  -> 提交 runner (scheduler 或 local thread)
      -> try/catch 包裹 ExecuteInternal
      -> 提交唯一终态(COMPLETED/FAILED/CANCELLED)
      -> 递减 inflight 计数并通知析构等待器
~DispensingExecutionUseCase
  -> 标记 stopping
  -> 请求 active task cancel + hardware stop
  -> 等待 inflight==0
  -> 释放资源
```

### 3.2 监控线程生命周期

```text
StartStatusUpdate
  -> 若已运行: 请求旧线程停止并 join
  -> 启动新线程
StopStatusUpdate
  -> 设置 stop flag
  -> 统一在外部 join
~MotionMonitoringUseCase
  -> StopStatusUpdate(保证无 detach 残留线程)
```

### 3.3 控制接口语义收敛

```text
CancelTask
  -> 请求 cancel
  -> scheduler cancel 仅表示调度层接受，不直接宣告终态
  -> 必须结合执行层停止确认后提交 CANCELLED
Pause/Resume
  -> 轮询中若观测到终态，返回终态映射错误/成功（非统一 timeout）
Cleanup
  -> 仅清理终态且超过 TTL 的任务
```

## 4. 子任务拆解（可直接实施）

### 子任务 A：DispensingExecution 生命周期与终态一致性
1. 引入 in-flight 任务计数与析构等待机制，覆盖 scheduler/local 两条路径。
2. 线程入口统一 `try/catch`，异常映射为 `FAILED` 并落错误上下文。
3. `CancelTask` 改为“请求取消 + 停止确认 + 终态提交”三段式，不再以 scheduler 返回值直接判定终态。
4. `PauseTask/ResumeTask` 在观测终态时返回语义化结果（completed/cancelled/failed）。
5. `CleanupExpiredTasks` 只清理终态任务；运行中任务仅告警不删除。

### 子任务 B：DispensingWorkflow 停止幂等与竞态收敛
1. `StopJob` 对取消失败补查真实任务状态，做幂等归并。
2. `RunJob` 中 stop/pause 竞态统一走终态提交函数，避免“返回失败但已停止”。
3. 错误消息追加 failure_stage，便于上层定位（cancel_forward / status_query / finalize 等）。

### 子任务 C：MotionMonitoring 线程安全修复
1. 移除 self-thread `detach` 分支，改为统一外层 join 或可中断线程模型。
2. 明确 `IsStatusUpdateRunning` 的状态定义：与线程真实存活一致。
3. 对采集失败建立诊断出口（失败计数、阶段标签或错误回调）。

### 子任务 D：ValveCoordination 安全校验硬化
1. `CheckValveSafety` 未实现项改为阻断（`NOT_IMPLEMENTED` 或明确安全失败码）。
2. 补接最小可用检查（硬件连接、急停、关键互锁、供胶阀状态）。
3. 保留与后续完整安全策略兼容的接口形态，避免重复改签名。

### 子任务 E：Redundancy 存储可靠性与输入语义
1. `SaveStoreAtomically` 从“remove+rename”改为可恢复替换策略，避免中间窗口丢数据。
2. `ListCandidates` 对非法 `priority` 输入返回 `INVALID_PARAMETER`，不再静默空结果。
3. 保持现有 rollback failure_stage 语义，并统一错误格式模板。

## 5. 失败模式与补救策略

1. **析构阻塞过久**：设置等待超时与告警；超时仍保持“拒绝新任务+持续停机流程”。
2. **取消确认失败**：返回包含 stage 的失败，禁止报告 `cancelled` 假成功。
3. **线程停不干净**：析构前二次检查活线程并记录栈线索，测试中强断言无泄漏线程。
4. **存储替换失败**：保留旧文件并返回 `FILE_IO_ERROR`，不进入部分写入状态。

## 6. 发布与回滚策略

1. 发布顺序：A(Execution) -> B(Workflow) -> C(Monitoring) -> D(ValveSafety) -> E(Redundancy)。
2. 每个子任务独立提交并附带对应测试，出现回归可按子任务粒度回滚。
3. 回滚优先级：若运行安全受影响，优先回滚 D；若稳定性受影响，优先回滚 A/C。
4. 合并门槛：P0/P1 必须清零，且回归集全绿后方可进入发布流程。

