# NOISSUE Wave 6B External Input Backfill and Observation Reopen Plan

- 状态：Done with Exception（Local Acceptance）
- 日期：2026-03-23
- 分支：feat/dispense/NOISSUE-wave6b-external-observation-reopen
- 工作区：`D:\Projects\SS-dispense-align-wave6a`
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260323-104249`
- 上一阶段 PR：`https://github.com/Xike-yuhuofei/SiligenPlatform/pull/4`（已合并）

## 1. 阶段目标

1. 完成 Wave6A 正式收口（PR 合并与门禁快照归档）。
2. 完成 Wave6B 外部输入补齐与外部观察重开，形成可审计证据链。
3. 完成 CI 门禁治理强化并验证 `run_apps` 条件执行行为。
4. 最终达成：Wave6B PR 合并 + main 最小复验。

## 2. 已完成执行（按工作包）

1. `WP-1` Wave6A 收尾门禁确认：
   - PR #4 已合并，merge commit=`822a4f53691decef52a80ebfd625c5a9a5aaf441`。
   - 最终 run `23418159551` checks 全部 `pass`。

2. `WP-3` 门禁治理强化：
   - 提交 `75731046`：`workspace-validation.yml` 增强 `push` 首提交 diff 兜底（`git show` fallback）。
   - 并发分组键改为分支名归一：`workspace-gate-${{ github.event.pull_request.head.ref || github.ref_name }}`。

3. `WP-4` 外部输入补齐与观察重开：
   - 证据根：`integration/reports/verify/wave6b-external-20260323-104249/`
   - 三类 intake 均 `collected`：
     - `field-scripts`
     - `release-package`
     - `rollback-package`（由 `generate-temporary-rollback-sop.ps1` 生成临时目录）
   - 外部观察执行：`run-external-migration-observation.ps1` exit `0`
   - 结果：四个 scope 均 `Go`，`external migration complete declaration = Go`

## 3. 特例说明与剩余任务

1. 特例背景：GitHub Actions 账户账单/额度问题导致 workflow_dispatch 未启动 runner。
   - run `23419203726`（`run_apps=false`）失败，annotation 显示 billing 限制。
   - run `23419226121`（`run_apps=true`）同类失败，jobs 未实际执行测试步骤。
   - PR `#6` run `23419522017` 同类失败（`legacy-exit-gates` / `detect-apps-scope` 在启动前失败）。

2. 特例决策（已执行）：
   - 停用 `workspace-validation.yml` 的 `push/pull_request` 自动触发，保留 `workflow_dispatch` 手动触发；
   - 以本地验证链 + 外部观察证据链作为本阶段验收依据。
3. 剩余任务（非阻断）：
   - 账单恢复后按需重跑两次 dispatch（`run_apps=false/true`）补云端行为证据。

## 4. 关键证据索引

1. 外部观察总览：
   - `integration/reports/verify/wave6b-external-20260323-104249/observation/summary.md`
   - `integration/reports/verify/wave6b-external-20260323-104249/observation/blockers.md`
2. 三类 intake：
   - `integration/reports/verify/wave6b-external-20260323-104249/intake/field-scripts.{md,json}`
   - `integration/reports/verify/wave6b-external-20260323-104249/intake/release-package.{md,json}`
   - `integration/reports/verify/wave6b-external-20260323-104249/intake/rollback-package.{md,json}`
3. rollback 生成记录：
   - `integration/reports/verify/wave6b-external-20260323-104249/intake/temporary-rollback-sop-generation.{md,json}`
