# System Acceptance Report

更新时间：`2026-04-04`

## 1. 结论

- 工作区已进入单轨：`apps/`、`modules/`、`shared/`、`docs/`、`samples/`、`tests/`、`scripts/`、`config/`、`data/`、`deploy/`。
- 旧根 `packages/`、`integration/`、`tools/`、`examples/` 已物理删除，并由 legacy-exit 门禁防回流。
- `specs/` 与 `.specify/` 已从当前工作区正式基线卸载；它们只允许作为历史快照或已删除资产被记录，不再作为活动 feature / support roots。
- 根级执行链统一为 `build.ps1`、`test.ps1`、`ci.ps1`、`scripts/validation/run-local-validation-gate.ps1`。
- 本次 legacy/bridge 收口已完成，closeout 判定为 `PASS`。

## 2. Speckit Exit 治理复核（2026-04-04）

| 检查项 | 结果 | 证据 |
|---|---|---|
| legacy fact catalog 对齐 | `PASS`（`specs/.specify -> removed`） | `scripts/migration/legacy_fact_catalog.json`；`tests/contracts/test_bridge_exit_contract.py` |
| bridge-exit contract | `PASS`（`13 passed`） | `python -m pytest .\\tests\\contracts\\test_bridge_exit_contract.py -q` |
| layered validation contracts | `PASS`（`10 passed`） | `python -m pytest .\\tests\\contracts\\test_layered_validation_contract.py .\\tests\\contracts\\test_layered_validation_lane_policy_contract.py -q` |
| workspace layout gate | `PASS`（`missing_key_count=0`, `legacy_root_present_count=0`, `bridge_root_failure_count=0`） | `python scripts/migration/validate_workspace_layout.py` |
| legacy-exit gate | `PASS with controlled exceptions`（`finding_count=11`, `blocking_count=0`） | `python scripts/migration/legacy-exit-checks.py --profile local --report-dir tests/reports/legacy-exit-current`；`tests/reports/legacy-exit-current/legacy-exit-checks.md` |
| root test entry contracts suite | `PASS`（`passed=22`, `failed=0`, `known_failure=0`, `skipped=0`） | `powershell -NoProfile -ExecutionPolicy Bypass -File .\\test.ps1 -Profile CI -Suite contracts -FailOnKnownFailure`；`tests/reports/workspace-validation.md`；`tests/reports/validation-evidence-bundle.json` |

- 当前 `legacy-exit` 的 `11` 条 finding 全部为 `controlled-exception`，来源是受控保留的 `tools/testing/check_no_loose_mock.py` 和已登记的模块级测试子目录引用，不构成 blocker。
- 历史过程文档中残留的 `specs/` / `.specify/` 表述按“历史快照”保留；它们不是当前默认入口，也不作为活动执行依据。
- `scripts/validation/run-local-validation-gate.ps1 -Lane quick-gate -DesiredDepth quick` 在本轮外层命令超时窗口内未完整收口，因此本次 speckit exit acceptance 不把 quick gate 写成已通过。

## 3. 本次复核证据（2026-03-25）

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

## 4. 对外口径

- 任何“当前来源/迁移来源”表述不得再把已删除旧根写成现势 owner。
- 文档、脚本、排障、部署说明统一指向单轨根与根级入口。
- 历史迁移过程材料仅允许保留在归档目录，不再作为执行依据。
- 历史过程文档若保留 `specs/` / `.specify/` 文案，必须被理解为历史快照，不得覆盖当前 `canonical-paths`、`workspace-baseline` 与 `system-acceptance-report` 的正式口径。

## 5. 历史材料归档

- 历史波次与迁移过程：`docs/_archive/`
- 冻结文档与验收索引：`docs/architecture/dsp-e2e-spec/`
- 历史验证报告：`tests/reports/verify/`（仅审计用途）

## 6. 后续基线

- 新增或改动必须满足：`known_failure=0`、`legacy-exit blocking_count=0`，且任何残留 finding 都必须是已登记的 `controlled-exception`。
- e2e/performance 回归报告在单轨目录下产出并固化到 `tests/reports/` 与 `docs/architecture/`。
- mock / 无机台回归通过后，仍需单独补真实机台或高保真仿真放行证据，覆盖运动精度、停止距离、真实 IO 与安全链。
- 若重新引入旧根、bridge metadata 或 bridge 路径文案，`validate_workspace_layout.py` 与 `legacy-exit-checks.py` 必须直接失败。
