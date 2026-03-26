# NOISSUE Wave 4D Scope - Launcher Gate Correction and Repo-Side Blocker Payoff

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-212031`
- 上游收口：`docs/process-model/plans/NOISSUE-wave4c-closeout-20260322-212031.md`

## 1. 阶段目标

1. 修正外部观察文档中错误的 canonical preview CLI 名称。
2. 把 launcher 验收从“PATH 可见性”修正为“目标 Python 能运行 canonical module CLI”。
3. 用官方安装脚本补当前机器的 canonical `engineering-data` 安装面，并产出 repo-side launcher 通过证据。
4. 为后续真实外部输入采集补 intake 模板，但不伪造外部包证据。

## 2. in-scope

1. `docs/runtime/external-migration-observation.md`
2. `tools/scripts/verify-engineering-data-cli.ps1`
3. `integration/reports/verify/wave4d-closeout/launcher/`
4. `integration/reports/verify/wave4d-closeout/intake/`
5. `docs/process-model/plans/`、`docs/process-model/reviews/` 中本阶段新增工件

## 3. out-of-scope

1. 回写 `Wave 4C` 历史工件
2. 补真实现场脚本、发布包、回滚包输入
3. 新的全量 build/test
4. 恢复任何 legacy 兼容壳

## 4. 完成标准

1. 活动文档只使用 canonical preview CLI 名称 `engineering-data-generate-preview`。
2. 新 helper 能生成 `engineering-data-cli-check.{json,md}` 并默认按解释器口径裁决。
3. `install-python-deps.ps1 -SkipApps` 后 helper 必需检查通过。
4. `legacy-exit-pre/post` 均通过。
5. `Wave 4D` closeout 明确写出：repo-side launcher blocker 已清除，但外部输入仍待补齐。
