# Implementation Plan: Activate Nightly Performance And Controlled HIL

**Branch**: `test/infra/TASK-007-activate-performance-and-controlled-hil` | **Date**: 2026-04-01 | **Spec**: `D:\Projects\ss-e\specs\test\infra\TASK-007-activate-performance-and-controlled-hil\spec.md`
**Input**: Feature specification from `D:\Projects\ss-e\specs\test\infra\TASK-007-activate-performance-and-controlled-hil\spec.md`

## Summary

本波次不再继续扩展分层测试平台，而是把 `TASK-006` 已完成的能力投入正式运行。实施分两条主线：

1. 修正 `nightly-performance` authority path 的 runtime artifact 解析，并冻结正式 threshold config。
2. 修正 `limited-hil` controlled path 的 blocked closeout 缺口，让 quick gate 即使失败也能形成完整 evidence。

## Technical Context

**Language/Version**: PowerShell 7、Python 3.11+、Markdown  
**Primary Dependencies**: `build.ps1`、`test.ps1`、`tests/performance/collect_dxf_preview_profiles.py`、`tests/e2e/hardware-in-loop/run_hil_controlled_test.ps1`、`verify_hil_controlled_gate.py`、`render_hil_controlled_release_summary.py`  
**Storage**: `tests/baselines/performance/`、`tests/reports/performance/`、`tests/reports/verify/`、`docs/validation/`、`specs/test/infra/`  
**Testing**: `tests/contracts/test_performance_threshold_gate_contract.py`、正式 `nightly-performance` 运行、正式 `limited-hil` quick gate 运行  
**Target Platform**: Windows 开发机；`nightly-performance` 使用 mock runtime；`limited-hil` 依赖真实控制卡/联机环境  
**Constraints**: 不新增 lane / runner / schema；不改业务模块；HIL quick gate 未通过时不得进入 formal `1800s` gate  

## Contract Freeze

- `nightly-performance` authority 仍是 `tests/performance/collect_dxf_preview_profiles.py`
- `limited-hil` authority 仍是 `tests/e2e/hardware-in-loop/run_hil_controlled_test.ps1`
- evidence schema 仍是 `validation-evidence-bundle.v2`
- report schema 仍是 `validation-report-manifest.v1` / `validation-report-index.v1`

## Implementation Changes

### 1. Nightly Performance Activation

- 让 `collect_dxf_preview_profiles.py` 的默认 gateway executable 解析顺序对齐根级 `build.ps1` 产物 reality。
- 为默认解析顺序补最小合同测试，防止 future regression 再次只认 `<repo-root>\build\bin\Debug`。
- 用 `small` sample、`cold/hot/singleflight`、`--include-start-job` 完成正式校准。
- 将 threshold config 从 smoke 级宽松值收紧为可审计的 blocking thresholds，并附 calibration evidence path。

### 2. Controlled HIL Activation

- 修正 `run_hil_controlled_test.ps1` 的 offline prerequisite `-Suite` 绑定方式，确保受控 HIL 真正消费 `contracts + e2e + protocol-compatibility`。
- 让 controlled script 在 offline prerequisites 已通过后，即使 HIL 步骤阻塞，仍继续执行 gate/release summary closeout。
- 执行一次 `60s` quick gate；若通过才允许进入 formal `1800s`，否则记录 blocked evidence 并停止。

### 3. Truth Sync

- 新建 Phase 12 closeout 文档，回填 performance pass / HIL blocked 结论与证据路径。
- 在 `build-and-test.md`、`validation/README.md`、`layered-test-matrix.md`、`tests/performance/README.md`、`tests/e2e/hardware-in-loop/README.md`、`release-test-checklist.md` 中同步新的 authority 真值。
- 在 `TASK-006` 账本中增加 follow-up 指针，并把后续工作切到 `TASK-007`。

## Validation Plan

1. `python .\tests\contracts\test_performance_threshold_gate_contract.py`
2. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile Local -Suite apps`
3. `python .\tests\performance\collect_dxf_preview_profiles.py --sample-labels small --cold-iterations 2 --hot-warmup-iterations 1 --hot-iterations 2 --singleflight-rounds 2 --singleflight-fanout 3 --include-start-job --gate-mode nightly-performance --threshold-config .\tests\baselines\performance\dxf-preview-profile-thresholds.json`
4. `.\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 -Profile Local -SkipBuild -UseTimestampedReportDir -HilDurationSeconds 60 -ReportDir tests\reports\verify\phase12-limited-hil -PublishLatestOnPass:$false`

## Risks

- `limited-hil` 是否能得到 `passed` 取决于真实控制卡/联机环境，本波次允许以 blocked evidence 收尾。
- 当前正式 calibration 只覆盖 `small` sample；`medium/large` 仍需后续单独冻结。
