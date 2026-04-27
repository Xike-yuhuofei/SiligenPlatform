# Premerge Review Report (Post-Fix)

- branch: `feat/hmi/NOISSUE-dispense-trajectory-preview-v2`
- base: `main`
- baseline synced: `git fetch origin main` (done)
- scope: `packages/process-runtime-core` 本轮 P1 修复复核
- generated_at: `2026-03-21`

## Findings

### P2-1 冗余存储恢复仍未覆盖“主文件可打开但 JSON 损坏”路径（非阻断）
- files:
  - `packages/process-runtime-core/src/infrastructure/adapters/redundancy/JsonRedundancyRepositoryAdapter.cpp`
- 风险:
  - 主文件内容损坏时直接返回 `JSON_PARSE_ERROR`，不会尝试 `.bak` 恢复。
- 触发条件:
  - 异常掉电或外部覆盖导致 JSON 内容损坏。
- 建议修复:
  - 在 parse failure 分支追加 `.bak` 读取与恢复；补回归用例。

## Open Questions

1. `ITaskSchedulerPort` 是否需要明确“`NOT_FOUND` 等价终态不可再执行”的契约文档，以固化 in-flight reconcile 的边界。

## Residual Risks

- `DispensingExecutionUseCase` 析构阶段目前采用“有界等待 + 调度器对账 + 重试等待”，可显著降低 pending 任务导致的卡死概率，但在底层调度器持续失步时仍可能长时间阻塞（当前策略为安全优先）。
- `StartJob` 线程启动失败回滚已完成；该失败路径缺少可稳定触发的单测注入点（已通过全量编译与回归验证主路径）。

## 建议下一步

1. 可进入提交流程。
2. 若要进一步消除残余风险，建议后续补 scheduler 故障注入测试与 `.bak` parse-fallback 修复。

