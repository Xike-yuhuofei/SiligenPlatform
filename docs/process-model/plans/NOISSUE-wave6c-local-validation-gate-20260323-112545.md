# NOISSUE Wave 6C Local Validation Gate Plan

- 状态：Completed
- 日期：2026-03-23
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260323-112545`

## 1. 目标与口径

1. 在私有仓库 GitHub Actions billing 受限场景下，将默认验收门禁切换为本地串行验证链路。
2. 保留一次性可追溯证据（json、md、分步日志），用于替代云端 checks 结果。
3. 明确迁移策略为“本地先行、最终阶段至少一次可复现实跑证据”，不再依赖 GitHub Actions。

## 2. 子任务拆分

1. 子任务 A（本次先做）：新增统一本地门禁脚本并固化输出结构。
2. 子任务 B：禁用现有 GitHub Actions workflow，避免继续触发计费流水线。
3. 子任务 C：更新发布/验收流程文档，统一入口为本地门禁脚本。
4. 子任务 D：执行一次本地门禁实跑，生成并归档证据。
5. 子任务 E：补充 QA/Release 收口评审文档，给出可合并结论。

## 3. 子任务 A/B 实施记录（已完成）

1. 新增脚本：`tools/scripts/run-local-validation-gate.ps1`
2. 脚本串行执行以下步骤并输出 summary：
   - `python .\tools\migration\validate_workspace_layout.py`
   - `test.ps1 -Profile CI -Suite packages -FailOnKnownFailure`
   - `tools/build/build-validation.ps1 -Profile Local -Suite packages`
   - `tools/build/build-validation.ps1 -Profile CI -Suite packages`
3. 已将 `.github/workflows/workspace-validation.yml` 迁移为：
   - `.github/workflows-disabled/workspace-validation.yml.disabled`

## 3.1 子任务 C 实施记录（已完成）

1. 已更新发布与门禁入口文档，统一默认验收口径为本地门禁脚本：
   - `docs/runtime/release-process.md`
   - `docs/architecture/build-and-test.md`
   - `docs/architecture/governance/migration/legacy-exit-checks.md`
   - `docs/onboarding/developer-workflow.md`
2. 文档明确：
   - 默认验收命令为 `tools/scripts/run-local-validation-gate.ps1`
   - `workspace-validation` GitHub Actions workflow 已禁用并迁移到 `workflows-disabled`

## 4. 证据产物规范

1. 执行目录：`integration/reports/local-validation-gate/<timestamp>/`
2. 统一摘要：
   - `local-validation-gate-summary.json`
   - `local-validation-gate-summary.md`
3. 分步日志：`logs/01-*.log` 到 `logs/04-*.log`

## 5. 收口结果

1. 已执行：
   - `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\scripts\run-local-validation-gate.ps1`
2. 本次实跑证据：
   - `integration/reports/local-validation-gate/20260323-113714/local-validation-gate-summary.json`
   - `integration/reports/local-validation-gate/20260323-113714/local-validation-gate-summary.md`
3. 实跑结论：
   - `overall_status=passed`
   - `passed_steps=4/4`
