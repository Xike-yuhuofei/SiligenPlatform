# System Acceptance Report 2026-04-07

本页保存 `2026-04-07` 版 `system-acceptance-report` 的历史复核证据与按日期展开的 acceptance 细节。  
当前正式结论请优先阅读 [docs/architecture/system-acceptance-report.md](/D:/Projects/SiligenSuite/docs/architecture/system-acceptance-report.md)。

## 1. Speckit Exit 治理复核（2026-04-04）

| 检查项 | 结果 | 证据 |
|---|---|---|
| legacy fact catalog 对齐 | `PASS`（`specs/.specify -> local-cache`） | `scripts/migration/legacy_fact_catalog.json`；`tests/contracts/test_bridge_exit_contract.py` |
| bridge-exit contract | `PASS`（`13 passed`） | `python -m pytest .\\tests\\contracts\\test_bridge_exit_contract.py -q` |
| layered validation contracts | `PASS`（`10 passed`） | `python -m pytest .\\tests\\contracts\\test_layered_validation_contract.py .\\tests\\contracts\\test_layered_validation_lane_policy_contract.py -q` |
| workspace layout gate | `PASS`（`missing_key_count=0`, `legacy_root_present_count=0`, `bridge_root_failure_count=0`） | `python scripts/migration/validate_workspace_layout.py` |
| legacy-exit gate | `PASS with controlled exceptions`（`finding_count=11`, `blocking_count=0`） | `python scripts/migration/legacy-exit-checks.py --profile local --report-dir tests/reports/legacy-exit-current`；`tests/reports/legacy-exit-current/legacy-exit-checks.md` |
| root test entry contracts suite | `PASS`（`passed=22`, `failed=0`, `known_failure=0`, `skipped=0`） | `powershell -NoProfile -ExecutionPolicy Bypass -File .\\test.ps1 -Profile CI -Suite contracts -FailOnKnownFailure`；`tests/reports/workspace-validation.md`；`tests/reports/validation-evidence-bundle.json` |

- 当前 `legacy-exit` 的 finding 仅来自已登记的模块级测试子目录引用，不构成 blocker；`no-loose-mock` 已迁到 `scripts/testing/check_no_loose_mock.py`，不再依赖 `tools/` 例外。
- `bridge-exit contract` 的 `PASS` 仅表示根级 bridge/legacy contract 契约通过，不等价于 `runtime-execution` / `runtime-service` owner closeout 已完成；当前正式状态以 `docs/architecture/legacy-cutover-status.md` 为准。
- `2026-04-07` 已完成 runtime contracts alias shell 清零：`IConfigurationPort` / `ITaskSchedulerPort` / `IEventPublisherPort` 不再是 live code 或 `contracts/runtime` canonical required surface 的一部分。
- 历史过程文档中残留的 `specs/` / `.specify/` 表述按“历史快照”保留；当前工作区中的同名目录若存在，也只视为本地缓存，不作为活动执行依据。
- `scripts/validation/run-local-validation-gate.ps1 -Lane quick-gate -DesiredDepth quick` 在本轮外层命令超时窗口内未完整收口，因此本次 speckit exit acceptance 不把 quick gate 写成已通过。

## 2. 本次复核证据（2026-03-25）

| 检查项 | 结果 | 证据 |
|---|---|---|
| workspace layout gate | `PASS`（`missing=0`, `legacy_root_present=0`, `bridge_root_failure=0`） | `python scripts/migration/validate_workspace_layout.py`；`tests/reports/workspace-validation.md` |
| legacy-exit gate | `PASS`（`finding_count=0`） | `tests/reports/legacy-exit-current/legacy-exit-checks.md` |
| freeze/matrix/bridge/application/engineering contracts | `PASS`（`passed=15`, `failed=0`, `known_failure=0`, `skipped=0`） | `powershell -File .\\test.ps1 -Profile CI -Suite contracts -FailOnKnownFailure`；`tests/reports/workspace-validation.md` |
| local validation gate | `PASS`（`overall_status=passed`, `passed_steps=6/6`） | `tests/reports/local-validation-gate/20260325-212745/local-validation-gate-summary.md` |
| formal freeze docset | `PASS`（`missing_doc_count=0`, `finding_count=0`） | `tests/reports/dsp-e2e-spec-docset/dsp-e2e-spec-docset.md` |
| build-validation（Local/CI contracts） | `PASS` | `tests/reports/local-validation-gate/20260325-212745/local-validation-gate-summary.md` |

### 无机台运行链补充复核（北京时间 `2026-03-26 00:31`，对应 UTC `2026-03-25T16:31Z`）

| 检查项 | 结果 | 证据 |
|---|---|---|
| first-layer rereview | `PASS`（`overall_status=passed`） | `tests/reports/verify/release-validation-20260326-002954/phase4-first-layer-rereview/20260325T163158Z/first-layer-rereview-summary.md` |
| door interlock | `PASS`（`D4-D8=passed`） | `tests/reports/verify/release-validation-20260326-002954/phase4-first-layer-rereview/20260325T163158Z/s5-door-interlock/tcp-door-interlock.md` |
| negative-limit chain | `PASS`（`L4-L6=passed`） | `tests/reports/verify/release-validation-20260326-002954/phase4-first-layer-rereview/20260325T163158Z/s6-negative-limit-chain/tcp-negative-limit-chain.md` |
| hmi external-gateway smoke | `PASS`（`online_ready=true`） | `tests/reports/verify/release-validation-20260326-002954/phase4-hmi-online-smoke/online-smoke.log` |
| hmi protocol/run-script unit contracts | `PASS`（`16 + 5`） | `tests/reports/verify/release-validation-20260326-002954/phase4-hmi-unit/test_protocol_preview_gate_contract.log`；`tests/reports/verify/release-validation-20260326-002954/phase4-hmi-unit/test_run_scripts.log` |

- 本轮无新增失败；正式批次汇总见 `tests/reports/verify/release-validation-20260326-002954/phase4-mock-regression-summary.md`。
- `mock.io.set(door=...)` 后 `status` 首帧存在滞后，当前权威口径是轮询到 `machine_state=Fault/Idle` 收敛，不对首帧硬断言。
- `mock.io.set(limit_x_neg/limit_y_neg=true)` 当前会驱动 HOME boundary 并拦截负向运动，但 `status.io.limit_x_neg/limit_y_neg` 仍不暴露该状态；这仍是未覆盖/待决策项。
- 本轮结论仅代表 mock / 无机台验证通过，不等同于真实机台现场通过。
