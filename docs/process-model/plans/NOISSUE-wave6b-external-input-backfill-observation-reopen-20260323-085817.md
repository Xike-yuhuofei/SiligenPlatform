# NOISSUE Wave 6B External Input Backfill and Observation Reopen Plan

- 状态：In Progress
- 日期：2026-03-23
- 分支：feat/dispense/NOISSUE-wave6a-foundation-ingest
- 工作区：`D:\Projects\SS-dispense-align-wave6a`
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260323-085817`
- 上一阶段 PR：`https://github.com/Xike-yuhuofei/SiligenPlatform/pull/4`

## 1. 阶段目标

1. 完成 Wave6A 收尾门禁确认并形成可合并结论（不使用 admin bypass）。
2. 推进 Wave6B：外部输入补齐与外部观察重开，形成可追踪证据链。
3. 降低 PR 门禁等待成本，重点治理 `validation (apps)` 长耗时对交付节奏的阻塞。

## 2. 已知事实与约束

1. `gh` 在本机优先读取环境变量 `GITHUB_TOKEN`；若该 token 权限受限，会导致 PR/checks 查询失败（404/GraphQL not found）。
2. `validation (apps)` 在当前流水线中为长耗时路径，常见执行时长约 30 分钟，显著拖慢收尾确认。
3. 当前工作分支已合规命名且已推送，禁止 destructive git 命令，禁止覆盖他人改动。

## 3. 工作包拆解（多子任务）

1. `WP-1` Wave6A 收尾门禁确认
   - 拉取 PR #4 checks 实时状态，输出是否满足可合并条件。
   - 若失败：定位到具体 job/step，最小修复并补齐验证与文档证据。
   - 交付：收尾结论与阻塞项清单（如有）。

2. `WP-2` 认证路径收敛（本机 gh 可观测性稳定）
   - 约定 PR/check 查询命令在执行前显式清理当前进程的 `GITHUB_TOKEN`，回退到 keyring 凭据。
   - 在流程文档中固化“token 优先级与排障步骤”。
   - 验收：`gh pr view/checks` 可稳定读取私有仓库 PR 状态。

3. `WP-3` 门禁提速（`validation (apps)` 治理）
   - 评估并实施路径过滤：仅当 `apps/**` 或相关构建脚本变更时触发 apps lane。
   - 引入 `concurrency`（按分支分组，`cancel-in-progress: true`）减少重复排队。
   - 将慢任务拆分为“PR 必需最小集”与“非阻断扩展集（nightly/手动触发）”的改造方案。
   - 验收：非 apps 改动的 PR 不再被 apps lane 阻塞；同分支重复提交不叠加等待。

4. `WP-4` 外部输入补齐与外部观察重开
   - 补齐 field scripts/release evidence/rollback package 相关输入。
   - 恢复外部观察执行并产出可审计报告。
   - 验收：形成完整外部输入与观察证据链，支持下一轮 release 审查。

## 4. 执行顺序与里程碑

1. M1（进行中）：Wave6A 收尾门禁确认。
2. M2：落地认证路径收敛（命令级约束 + 文档固化）。
3. M3：完成 workflow 提速改造并通过一次 PR 触发验证。
4. M4：完成外部输入补齐与外部观察重开证据归档。

## 5. 风险与应对

1. 风险：`validation (apps)` 长时间运行导致收尾卡点。
   - 应对：拆分门禁、路径过滤、并发取消；保留扩展验证在非阻断通道执行。
2. 风险：认证来源混用导致 checks 不可见。
   - 应对：统一排障动作（先清理 `GITHUB_TOKEN` 再执行 gh 查询），在文档中固定为标准步骤。
3. 风险：外部输入补齐信息不完整。
   - 应对：先建立输入清单与缺口矩阵，再分批补齐并逐项验证。

## 6. 第一阶段执行记录

1. 已完成第一步：本计划文档已落盘。
2. 已执行 `WP-3` 首轮改造：`.github/workflows/workspace-validation.yml` 新增 `workflow_dispatch`、`concurrency`、`detect-apps-scope` 路径过滤，并在 `validation` job 对 `apps` suite 做条件执行。
3. 下一步：将上述改造推送并在 PR/手动触发场景验证 `apps` 过滤与并发取消行为，随后继续跟踪 PR #4 直到给出 Wave6A 可合并/阻塞结论。
