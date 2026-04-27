# NOISSUE Wave 2B Prep 收口报告

- 状态：Pass
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-153148`
- 关联报告根：`integration/reports/verify/wave2b-prep/`
- 关联 QA：`docs/process-model/reviews/feat-dispense-NOISSUE-e2e-flow-spec-alignment-qa-20260322-153148.md`

## 1. 收口范围

本次只收口 `Wave 2B` 物理切换前的准备项，不执行根级 source-root 翻转、不执行 runtime 默认资产路径切换，也不清零全部 residual dependency。

## 2. 当前代码事实

1. root layout 已收敛到 `cmake/workspace-layout.env`
2. `CMakeLists.txt`、`tests/CMakeLists.txt`、`tools/build/build-validation.ps1`、`packages/test-kit/src/test_kit/workspace_validation.py` 已通过 loader/script/module 读取同一份 layout
3. runtime 默认资产路径解析已收敛到 `packages/runtime-host/src/runtime/configuration/WorkspaceAssetPaths.h`
4. `apps/control-runtime`、`apps/control-tcp-server`、`apps/control-cli`、`packages/runtime-host`、`packages/transport-gateway` 的默认配置/recipe/schema 路径已汇聚到同一入口
5. residual dependency 账本已落盘到 `tools/migration/wave2b_residual_ledger.json`
6. `tools/migration/validate_wave2b_residuals.py` 已接入 `packages` suite，当前目标是阻止 live 引用新增，而不是要求历史文档提及立即归零
7. `shared-freeze` 门禁已同步补录 `packages/test-kit/src/test_kit/workspace_layout.py`，保证新增 layout 模块不会被误判为边界膨胀

## 3. 验证结果

1. `python tools/migration/validate_shared_freeze.py`
   - 结果：通过
2. `python tools/migration/validate_workspace_layout.py`
   - 结果：通过
3. `python tools/migration/validate_wave2b_residuals.py`
   - 结果：通过
4. `.\test.ps1 -Profile CI -Suite packages -ReportDir integration\reports\verify\wave2b-prep\packages -FailOnKnownFailure`
   - 结果：`passed=8 failed=0 known_failure=0 skipped=0`
5. `.\build.ps1 -Profile CI -Suite apps`
   - 结果：通过
6. `.\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\wave2b-prep\apps -FailOnKnownFailure`
   - 结果：`passed=16 failed=0 known_failure=0 skipped=0`
7. `.\build.ps1 -Profile CI -Suite integration,protocol-compatibility,simulation`
   - 结果：通过
8. `.\test.ps1 -Profile CI -Suite integration,protocol-compatibility,simulation -ReportDir integration\reports\verify\wave2b-prep\extended -FailOnKnownFailure`
   - 结果：`passed=7 failed=0 known_failure=0 skipped=0`

## 4. 结论

1. `Wave 2B Prep = Pass`
2. 当前工作树已满足“先建立单一来源、再加门禁、最后做跨层回归”的准备阶段目标
3. 当前结论不等于 `Wave 2B Readiness = Go`

## 5. 仍然明确 No-Go 的事项

1. 禁止直接翻转根级 `CMakeLists.txt` / `tests/CMakeLists.txt` 的 canonical source root
2. 禁止直接把 runtime 默认资产路径改写到新目录
3. 禁止在没有替代证据根的情况下拆分 `integration/reports`
4. 禁止把 residual ledger 已落地误解为 residual dependency 已完成迁移

## 6. 下一阶段允许推进的工作

1. `Wave 2B-A`：root build/test/source-root cutover 设计与回滚单元映射
2. `Wave 2B-B`：runtime 默认资产路径 bridge 与 fallback 设计
3. `Wave 2B-C`：residual dependency 逐项迁移 owner、清账与 gate 收紧
4. `Wave 2B-D`：把切换动作映射到根级 gate、报告路径与 fail-fast 规则
