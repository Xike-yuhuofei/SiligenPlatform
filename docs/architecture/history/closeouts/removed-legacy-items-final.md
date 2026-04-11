# Removed Legacy Items Final

更新时间：`2026-03-24`

## 1. 目的

本页记录模板化工作区重构中的迁移波次、legacy 删除结论和 migration-source roots 的退出台账。

## 2. 已保持删除状态的历史根

以下历史目录继续保持不存在，只允许作为退出结论出现：

- `control-core/`
- `hmi-client/`
- `dxf-editor/`
- `apps/dxf-editor-app/`
- `packages/editor-contracts/`
- 顶层 `simulation-engine/`
- `dxf-pipeline/`

## 3. migration-source roots 台账

| 来源根 | 目标根 | 当前结论 | 退出条件 |
|---|---|---|---|
| `packages/` | `modules/`、`shared/`、`apps/` | 保留为迁移来源，不再是默认 owner 面 | 新 owner 与正式入口不再落在 `packages/` |
| `integration/` | `tests/` | 保留现有报告与脚本来源 | 正式验证说明和新增入口迁到 `tests/` |
| `tools/` | `scripts/`、`deploy/` | 保留现有脚本来源 | 正式自动化和部署面迁到 `scripts/` / `deploy/` |
| `examples/` | `samples/` | 保留现有样本来源 | 长期复用样本迁到 `samples/` |

## 4. 迁移波次

| 波次 | 重点 | 当前结论 |
|---|---|---|
| Wave 0 | 统一冻结基线 | 已冻结阶段链、术语、失败语义与正式索引 |
| Wave 1 | canonical roots 与 shared-support | 已建立 `shared/`、`scripts/`、`tests/`、`samples/`、`deploy/` 的正式承载面 |
| Wave 2 | 上游静态工程链 `M1-M3` | 已将上游 owner 面收敛到 `modules/` 与 `samples/` |
| Wave 3 | 工作流与规划链 `M0/M4-M8` | 已完成 `process-runtime-core` 规划链 owner 拆分 |
| Wave 4 | 运行时执行链 `M9` | 已建立 `runtime-execution`、`runtime-service`、`runtime-gateway` 正式入口 |
| Wave 5 | `M10-M11`、`tests/`、`samples/` | 已完成 trace/HMI 与验证承载面收敛 |
| Wave 6 | 根级 cutover 与 legacy exit | 已完成 canonical build graph 切换，并确认 legacy roots 退出默认 owner |

## 5. bridge 退出规则

- `legacy-exit-checks.py` 持续阻止已删除目录、legacy 术语和 migration-source default-owner 口径回流。
- `validate_workspace_layout.py` 持续验证目标根、模块骨架和治理文档的同步存在。
- `validate_dsp_e2e_spec_docset.py` 持续验证 `S01-S10` 的冻结完整性和术语一致性。

## 6. closeout 证据

- `integration/reports/workspace-layout/us7-review-wave6-layout.txt`：`missing_key_count=0`，`failed_wiring_count=0`
- `tests/reports/dsp-e2e-spec-docset/`：冻结文档集校验已切换到当前唯一正式文档入口
- `integration/reports/workspace-build/us7-review-wave6-final-build.txt`：`workspace build complete`
- `integration/reports/workspace-test-wave6-rerun/workspace-validation.md`：`passed=52`，`failed=0`
- `integration/reports/workspace-ci-wave6/workspace-validation.md`：`passed=47`，`failed=0`
- `integration/reports/workspace-ci-wave6/legacy-exit/legacy-exit-checks.md`：`failed_rules=0`，`findings=0`
- `integration/reports/local-validation-gate-wave6/20260324-211140/local-validation-gate-summary.md`：`Overall: passed`
- `integration/reports/legacy-exit-wave6/legacy-exit-checks.md`：`failed_rules=0`，`findings=0`

## 7. closeout 结论

- `US7 / Wave 6` closeout 已成立，模板化工作区重构已进入 terminal closeout。
- `packages/`、`integration/`、`tools/`、`examples/` 继续保留历史报告、wrapper 或 tombstone，但已明确退出默认 owner 与正式承载面。
- 根级 `build.ps1`、`test.ps1`、`ci.ps1`、local validation gate、layout validator、freeze validator 与 dedicated legacy exit 均已回链通过。
- migration-source bridge 的最终退出说明已经具备可审计证据，accepted exceptions：`none`
