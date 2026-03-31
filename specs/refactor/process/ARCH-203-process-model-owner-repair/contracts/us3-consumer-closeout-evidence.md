# US3 Consumer Closeout Evidence

## Objective

统一 CLI、gateway、runtime-service、HMI host、协议测试、review supplements 与根级 closeout 证据，使当前 clean worktree 可以独立完成复查。

## Consumer Cutover Matrix

| Consumer | Planned Tasks | Canonical Surface Requirement | Status |
| --- | --- | --- | --- |
| `apps/planner-cli` | `T030` | 只依赖 canonical planning/execution public surface | `Verified by consumer file review` |
| `apps/runtime-gateway` | `T028`, `T030`, `T032` | 只依赖 canonical gateway-facing execution/planning surface | `Verified by targeted pytest and boundary review` |
| `apps/runtime-service` | `T030`, `T032`, `T036` | 只依赖 canonical bootstrap and execution surface | `Verified by consumer file review and root closeout` |
| `apps/hmi-app` | `T028`, `T031`, `T032` | 不再依赖 `client.preview_session` compat re-export 或 owner-level host logic | `Verified by targeted pytest` |
| protocol / contract tests | `T028`, `T029`, `T036` | 测试入口与真实 canonical surfaces 一致 | `Verified; supplement gate added` |

## Targeted Regression Ledger

| Command | Result | Notes |
| --- | --- | --- |
| `pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/validation/assert-module-boundary-bridges.ps1 -WorkspaceRoot . -ReportDir tests/reports/module-boundary-bridges-current` | `passed; finding_count=0` | `T030` cutover 后 boundary guard 仍保持绿色 |
| `pytest apps/runtime-gateway/transport-gateway/tests/test_transport_gateway_compatibility.py shared/contracts/application/tests/test_protocol_compatibility.py -q` | `27 passed` | gateway / shared protocol consumer-cutover regressions green |
| `pytest apps/hmi-app/tests/unit/test_m11_owner_imports.py apps/hmi-app/tests/unit/test_main_window.py -q` | `34 passed` | HMI owner import cutover and host thin-shell regressions green |
| `pytest apps/hmi-app/tests/unit/test_validation_contract_guard.py -q` | `3 passed` | HMI formal launcher contract guard green |
| `.\test.ps1 -Profile CI -Suite contracts -ReportDir tests/reports/arch-203 -FailOnKnownFailure` | `passed (15 passed, 0 failed, 0 known_failure)` | root closeout contracts suite green |

## T030 Closeout Review

| Consumer Slice | Reviewed Files | Closeout Conclusion |
| --- | --- | --- |
| `planner-cli` | `apps/planner-cli/CommandHandlers.Dxf.cpp` | `HandleDXFAugment` 已从内部 `ContourAugmenterAdapter` 头切到 `topology_feature/contracts/ContourAugmentContracts.h`，CLI 不再直接 include topology-feature 内部 adapter 路径 |
| `runtime-gateway` | `apps/runtime-gateway/transport-gateway/src/tcp/TcpCommandDispatcher.cpp`, `apps/runtime-gateway/transport-gateway/src/wiring/TcpServerHost.cpp` | dispatcher 的配置事实 include 已切到 `process_planning/contracts/ConfigurationContracts.h`；`TcpServerHost.cpp` 仅保留 `IConfigurationPort` 注入与 server/dispatcher 装配职责，不再承担 owner 逻辑 |
| `runtime-service` | `apps/runtime-service/bootstrap/ContainerBootstrap.cpp`, `apps/runtime-service/container/ApplicationContainer.h`, `apps/runtime-service/container/ApplicationContainer.cpp` | bootstrap/container 对配置事实的 include 已切到 `process_planning/contracts/ConfigurationContracts.h`；`ApplicationContainer.cpp` 继续只返回已注册的 config port 缓存，属于 `runtime_process_bootstrap` 受控宿主 surface，而非 `M4` live owner |

## Review Supplement Ledger

| Review File | Required Supplement | Target Task | Status |
| --- | --- | --- | --- |
| `docs/process-model/reviews/coordinate-alignment-module-architecture-review-20260331-074844.md` | hash, diff, command, precise index supplement | `T033` | `Supplemented` |
| `docs/process-model/reviews/dispense-packaging-module-architecture-review-20260331-074840.md` | hash, diff, command, precise index supplement | `T033` | `Supplemented` |
| `docs/process-model/reviews/topology-feature-module-architecture-review-20260331-075200.md` | hash, diff, command, precise index supplement | `T033` | `Supplemented` |
| `docs/process-model/reviews/process-planning-module-architecture-review-20260331-075201.md` | hash, diff, command, precise index supplement | `T033` | `Supplemented` |

## Root Closeout Commands

| Command | Result | Evidence |
| --- | --- | --- |
| `.\build.ps1 -Profile CI -Suite contracts` | `passed` | `tests/reports/arch-203/arch-203-root-entry-summary.md` |
| `.\test.ps1 -Profile CI -Suite contracts -ReportDir tests/reports/arch-203 -FailOnKnownFailure` | `passed (15 passed, 0 failed, 0 known_failure)` | `tests/reports/arch-203/workspace-validation.md` |
| `.\ci.ps1 -Suite contracts -ReportDir tests/reports/ci-arch-203` | `passed` | `tests/reports/ci-arch-203/workspace-validation.md`, `tests/reports/ci-arch-203/local-validation-gate/20260331-102855/local-validation-gate-summary.md` |
| `scripts/validation/run-local-validation-gate.ps1 -ReportRoot tests/reports/local-validation-gate` | `passed (8/8)` | `tests/reports/local-validation-gate/arch-203-blocked-summary.md` |

## Closure Notes

- `apps/hmi-app/config/gateway-launch.json` 已补齐，HMI formal launcher contract 不再是当前 root closeout 的阻断项。
- 本轮已删除 `apps/hmi-app/src/hmi_client/client/preview_session.py` 兼容旁路，并把主窗口 preview owner import 直接切到 `modules/hmi-application`。
- `T030` 与 root closeout 均已完成；`US3` 当前状态为“消费者收口与根级验收均已验证”。
