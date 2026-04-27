# NOISSUE Wave 5B Release Scope

- 状态：Prepared
- 日期：2026-03-23
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260323-000338`
- base 分支：`main`
- 对比基线：`origin/main...HEAD`（发布收口仅按白名单提交）

## 1. 发布目标

1. 完成 `Wave 4E -> Wave 5A -> Wave 5B` 证据链路入库，避免仅本地留存。
2. 基于已落盘 `wave5b` 证据完成发布门禁复验文档化，不重做已完成的 repo-side 修正。
3. 完成常规 push 与自动 PR 创建，输出最终 release 摘要。

## 2. 提交范围（白名单）

1. 外部观察/launcher 脚本：
   - `tools/scripts/register-external-observation-intake.ps1`
   - `tools/scripts/run-external-migration-observation.ps1`
   - `tools/scripts/verify-engineering-data-cli.ps1`
   - `tools/scripts/generate-temporary-rollback-sop.ps1`
2. 运行文档：
   - `docs/runtime/external-migration-observation.md`
3. 流程计划文档：
   - `docs/process-model/plans/*wave4e*`
   - `docs/process-model/plans/NOISSUE-wave5a-release-scope-20260322-234806.md`
   - `docs/process-model/plans/NOISSUE-wave5b-release-scope-20260323-000338.md`
4. 流程评审文档：
   - `docs/process-model/reviews/*wave4e*`
   - `docs/process-model/reviews/*234806*`
   - `docs/process-model/reviews/*000338*`

## 3. 非提交范围说明

1. 工作树中其他大量改动不纳入本次提交，严格按白名单 `git add`。
2. `integration/reports/**` 作为执行证据目录保留在工作区，不进入发布提交。
3. 不继续进行 `Wave 4B/4C/4D` 已完成的 repo-side 代码修正与重复验证。

## 4. Wave 5B 门禁口径

1. launcher CLI 复验：`verify-engineering-data-cli.ps1` 必须 `overall_status=passed`。
2. external observation recheck：四个 scope 必须全部 `Go`，且 `external migration complete declaration=Go`。
3. apps 回归：`test.ps1 -Profile CI -Suite apps -FailOnKnownFailure` 必须 `failed=0`。
