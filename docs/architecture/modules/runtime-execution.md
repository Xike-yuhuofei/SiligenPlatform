# runtime-execution

`M9 runtime-execution`

## 模块职责

- 承接 `S12-S15`，完成预检、预览、空跑、首件、执行、恢复和中止。
- 只消费上游冻结包，不回写规划事实。

## Owner 对象

- `MachineReadySnapshot`
- `ExecutionSession`
- `FirstArticleResult`
- 运行时事件流

## 输入契约

- `CreateExecutionSession`
- `RunPreflight`
- `RunPreview`
- `RunDryRun`
- `RunFirstArticle`
- `ApproveFirstArticle`
- `RejectFirstArticle`
- `WaiveFirstArticle`
- `StartExecution`
- `PauseExecution`
- `ResumeExecution`
- `RecoverExecution`
- `AbortExecution`

## 输出契约

- `ExecutionSessionCreated`
- `MachineReadySnapshotBuilt`
- `PreflightStarted`
- `PreflightBlocked`
- `PreflightPassed`
- `PreviewCompleted`
- `DryRunCompleted`
- `FirstArticlePassed`
- `FirstArticleRejected`
- `FirstArticleWaived`
- `FirstArticleNotRequired`
- `ExecutionStarted`
- `ExecutionPaused`
- `ExecutionResumed`
- `ExecutionRecovered`
- `ExecutionCompleted`
- `ExecutionAborted`
- `ExecutionFaulted`

## 禁止越权边界

- 不得重写 `ExecutionPackage`。
- 不得重建 `MotionPlan` 或 `ProcessPlan`。
- 不得把 `PreflightBlocked` 解释成上游规划失败。

## 测试入口

- `S09` preflight-blocked-case
- `S09` first-article-reject-case
- `S09` runtime-fault-recovery-case
