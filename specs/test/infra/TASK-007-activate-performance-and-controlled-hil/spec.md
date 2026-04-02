# Feature Specification: Activate Nightly Performance And Controlled HIL

**Feature Branch**: `test/infra/TASK-007-activate-performance-and-controlled-hil`
**Created**: 2026-04-01
**Status**: Completed
**Input**: User description: "在 TASK-006 平台建设完成后，把 nightly-performance 和 limited-hil 作为正式通道激活，并留下可审计证据"

## User Scenarios & Testing

### User Story 1 - 正式激活 nightly-performance (Priority: P1)

作为发布门禁维护者，我需要 `nightly-performance` 能在现有根级 build reality 上稳定找到 canonical runtime artifact，并使用正式 threshold config 给出 blocking 判定。

**Independent Test**: 运行 `tests/performance/collect_dxf_preview_profiles.py --include-start-job --gate-mode nightly-performance --threshold-config tests/baselines/performance/dxf-preview-profile-thresholds.json` 后，应生成 `threshold_gate + validation-evidence-bundle + report-manifest + report-index`，且结论可直接用于 nightly blocking gate。

### User Story 2 - 正式激活 limited-hil quick gate (Priority: P2)

作为联机验证维护者，我需要 `run_hil_controlled_test.ps1` 在 offline prerequisites 已通过后，无论真实 HIL quick gate 通过还是阻塞，都能生成完整 `gate + release-summary` 证据链，并据此决定是否允许进入 formal `1800s` gate。

**Independent Test**: 运行 `run_hil_controlled_test.ps1 -HilDurationSeconds 60` 后，要么得到 `passed`，要么得到带 `hil-controlled-gate-summary.*` 与 `hil-controlled-release-summary.md` 的 blocked 证据；若 quick gate 未通过，不得继续执行 formal `1800s` gate。

## Requirements

- **FR-001**: 本特性不得新增新的 root runner、lane、schema version 或 evidence owner。
- **FR-002**: `nightly-performance` 的正式 authority 继续由 `tests/performance/collect_dxf_preview_profiles.py` 承担。
- **FR-003**: `nightly-performance` 必须兼容根级 `build.ps1` 的 canonical build output reality，不能要求人工复制 runtime artifacts 到第二产物根。
- **FR-004**: `nightly-performance` 的正式 threshold config 必须明确样本范围、场景范围、reviewer guidance 和 calibration evidence。
- **FR-005**: 当 threshold config 包含 execution thresholds 时，正式 `nightly-performance` gate 必须包含 `--include-start-job`。
- **FR-006**: `limited-hil` 的正式 controlled path 继续由 `tests/e2e/hardware-in-loop/run_hil_controlled_test.ps1` 承担。
- **FR-007**: `limited-hil` quick gate 若在 offline prerequisites 通过后阻塞，仍必须产出 `hil-controlled-gate-summary.*` 与 `hil-controlled-release-summary.md`。
- **FR-008**: formal `1800s` HIL gate 只能在 quick `60s` gate 已通过后启动。
- **FR-009**: blocked HIL 结果必须明确区分 offline prerequisite failure、admission failure、safety preflight failure 与真实 hardware/HIL execution failure。

## Scope Boundaries

- 本特性只激活现有平台，不再扩展 `TASK-006` 的平台语义。
- 本特性不改业务 runtime/module 代码。
- 本特性允许最小修正 authority path、controlled script 参数绑定和 blocked evidence closeout 缺口。

## Success Criteria

- **SC-001**: `nightly-performance` 在当前工作区可直接运行并给出 blocking verdict。
- **SC-002**: canonical threshold config 不再停留在 smoke 级 `30000 ms` 宽松阈值。
- **SC-003**: `limited-hil` quick gate 无论 `passed` 还是 `blocked`，都能完整归档 gate/release summary。
- **SC-004**: 若 quick `60s` gate 未通过，formal `1800s` gate 不会被继续触发。
