# Premerge Review Report

- 分支：`feat/hmi/NOISSUE-dispense-trajectory-preview-v2`
- 基线：`main`
- 审查时间：`2026-03-22`
- 已执行基线同步：`git fetch origin main`（已执行）
- `git diff origin/main...HEAD`：无提交差异
- 本报告审查对象：当前工作区未提交改动（重点覆盖轨迹预览相关改动）

## 1. Findings

### P1 - 范围污染风险（非本需求改动与本次实现并存）
- 文件：`packages/process-runtime-core/src/infrastructure/adapters/redundancy/JsonRedundancyRepositoryAdapter.cpp`、`packages/runtime-host/src/security/*` 等
- 风险：若直接整包提交，可能把冗余治理/互锁安全的并行改动一并带入本次轨迹预览 PR，增加回归面与回滚复杂度。
- 触发条件：使用 `git add -A` 或未按功能拆分提交。
- 建议修复：按功能拆分提交集；本次 PR 仅保留轨迹预览链路文件（HMI/runtime/gateway/对应测试与必要文档）。

### P2 - 运行时重同步失败后的重试无终止策略
- 文件：`apps/hmi-app/src/hmi_client/ui/main_window.py`
- 风险：当 `plan_id` 在 runtime 侧已失效时，`_preview_state_resync_pending` 会持续保留为 `True`，每 2 秒发起一次 `dxf_preview_snapshot`，可能造成日志噪声和额外 RPC 压力。
- 触发条件：断连恢复或任务终态后触发重同步，且 runtime 返回持续失败（如 `plan not found`）。
- 建议修复：对可判定不可恢复错误（如 `NOT_FOUND/INVALID_STATE`）执行一次性降级处理（清空 pending、失效本地预览并提示用户重新 prepare）。

### P2 - 网关 `max_polyline_points` 夹断缺少专门回归用例
- 文件：`packages/transport-gateway/src/tcp/TcpCommandDispatcher.cpp`
- 风险：当前已实现上限夹断（4000）与告警日志，但缺少针对此行为的自动化断言，后续重构时可能回退。
- 触发条件：参数解析逻辑重构或命令分发重构。
- 建议修复：在 `packages/transport-gateway/tests/` 增补 `max_polyline_points` 越界输入的契约/兼容测试。

## 2. Open Questions

1. 本次 PR 是否严格限定为“轨迹预览功能”并剔除并行的 redundancy/runtime-host 变更？
2. 对于重同步失败（计划失效）场景，产品期望是“静默重试”还是“立即要求用户重新规划”？

## 3. Residual Risks

1. 点数上限固定为 `4000` 后，低性能 HMI 终端在高频重绘时仍可能出现短时卡顿（需现场基线验证）。
2. 当前审查以工作区差异为主，尚未形成相对 `origin/main` 的提交级审查证据（因为 `origin/main...HEAD` 为空）。

## 4. 建议下一步

1. 先拆分提交范围，避免非本需求改动混入本次 PR。
2. 按上述 P2 建议补齐重同步失败策略与网关夹断测试，再执行 `ss-qa-regression`。
3. 通过后再进入 PR 提交流程（不在本步骤执行 push/merge）。
