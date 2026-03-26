# System Acceptance Report

更新时间：`2026-03-26`

## 1. 结论

- 工作区已进入单轨：`apps/`、`modules/`、`shared/`、`docs/`、`samples/`、`tests/`、`scripts/`、`config/`、`data/`、`deploy/`。
- 旧根 `packages/`、`integration/`、`tools/`、`examples/` 已物理删除，并由 legacy-exit 门禁防回流。
- 根级执行链统一为 `build.ps1`、`test.ps1`、`ci.ps1`、`scripts/validation/run-local-validation-gate.ps1`。
- 本次 legacy/bridge 收口已完成，closeout 判定为 `PASS`。

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

## 3. 对外口径

- 任何“当前来源/迁移来源”表述不得再把已删除旧根写成现势 owner。
- 文档、脚本、排障、部署说明统一指向单轨根与根级入口。
- 历史迁移过程材料仅允许保留在归档目录，不再作为执行依据。

## 4. 历史材料归档

- 历史波次与迁移过程：`docs/_archive/`
- 冻结文档与验收索引：`docs/architecture/dsp-e2e-spec/`
- 历史验证报告：`tests/reports/verify/`（仅审计用途）

## 5. 后续基线

- 新增或改动必须满足：`known_failure=0`、`legacy-exit finding_count=0`。
- e2e/performance 回归报告在单轨目录下产出并固化到 `tests/reports/` 与 `docs/architecture/`。
- mock / 无机台回归通过后，仍需单独补真实机台或高保真仿真放行证据，覆盖运动精度、停止距离、真实 IO 与安全链。
- 若重新引入旧根、bridge metadata 或 bridge 路径文案，`validate_workspace_layout.py` 与 `legacy-exit-checks.py` 必须直接失败。
