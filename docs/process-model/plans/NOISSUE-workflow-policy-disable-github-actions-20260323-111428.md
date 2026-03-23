# NOISSUE Workflow Policy - Disable GitHub Actions

- 状态：Approved
- 日期：2026-03-23
- 分支：`chore/runtime/NOISSUE-disable-github-actions`
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260323-111428`

## 1. 决策

1. 由于私有仓库与 GitHub Actions billing/额度持续不稳定，项目从本决议起弃用 GitHub Actions 作为默认门禁执行面。
2. `.github/workflows/` 下不再保留可执行 workflow 文件，现有 workflow 已迁移到：
   - `.github/workflows-disabled/workspace-validation.yml.disabled`
3. 默认验收链路切换为本地执行与落盘证据：
   - `python .\tools\migration\validate_workspace_layout.py`
   - `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite packages -FailOnKnownFailure`
   - `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build\build-validation.ps1 -Profile Local -Suite packages`
   - `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build\build-validation.ps1 -Profile CI -Suite packages`

## 2. 适用范围

1. 适用于当前仓库后续阶段的默认开发/验收流程。
2. 如需恢复 GitHub Actions，必须通过新的流程决议文档显式批准后再迁回 `.github/workflows/`。

## 3. 交付与追踪要求

1. 每个阶段必须在 `docs/process-model/reviews/` 落盘 premerge/qa/release 结论。
2. 关键验收证据必须可追溯到本地命令、退出码和报告路径。
3. 非标准特例（例如跳过某类验证）必须在 release 文档中明确写出原因、风险和补动作。
