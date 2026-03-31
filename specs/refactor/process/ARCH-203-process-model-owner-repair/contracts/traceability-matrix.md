# ARCH-203 Traceability Matrix

## Current Scope

- Branch: `refactor/process/ARCH-203-process-model-owner-repair`
- Baseline status: `Wave 0` review pack 已导入并冻结在 `docs/process-model/reviews/`
- Execution state: `US1`、`US2`、`US3` 均已完成验证，根级入口与 local gate 已通过
- Root-entry blocker: `none`

## Requirement Traceability

| Requirement | Planned Implementation | Verification | Evidence | Status |
| --- | --- | --- | --- | --- |
| `FR-001` Review baseline and recheck report are the only fact inputs | `T004`, `T006`, `T007` | baseline inventory review, contracts index review | `docs/process-model/reviews/*20260331*.md`, `contracts/README.md`, `quickstart.md` | `Verified` |
| `FR-002` Freeze module charters for all in-scope modules | `T008`, `T010`-`T013` | module charter review, boundary guard | `module-owner-boundary-contract.md`, module `README.md` / `module.yaml`, `tests/reports/module-boundary-bridges-current/module-boundary-bridges.json` | `Verified` |
| `FR-003` Establish single owner mapping for handoff artifacts | `T001`, `T009`, `T014` | owner-map ledger review | `traceability-matrix.md`, `us1-owner-map-evidence.md` | `Verified` |
| `FR-004` Restrict `workflow` to orchestration and controlled compatibility | `T005`, `T018`, `T024` | boundary script, workflow/runtime CMake review | `us2-owner-closure-evidence.md`, `tests/reports/module-boundary-bridges-current/module-boundary-bridges.json` | `Verified` |
| `FR-005` Clarify `runtime-execution` execution ownership | `T016`, `T018`, `T027` | runtime host regression entry review, boundary script, root-entry summary | `us2-owner-closure-evidence.md`, `tests/reports/arch-203/arch-203-root-entry-summary.md` | `Verified` |
| `FR-006` Freeze the `M4 -> M9` handoff chain | `T017`, `T019`-`T023` | planning-chain CMake review, boundary script | `module-owner-boundary-contract.md`, `us2-owner-closure-evidence.md` | `Verified` |
| `FR-007` Clarify `topology-feature` owner surface | `T012`, `T013`, `T033`, `T034` | repo doc review, review supplement audit | `us1-owner-map-evidence.md`, `us3-consumer-closeout-evidence.md`, `docs/process-model/reviews/topology-feature-module-architecture-review-20260331-075200.md` | `Verified` |
| `FR-008` Separate `hmi-application` from host HMI responsibilities | `T028`, `T031`, `T032` | targeted pytest, owner-import review, formal contract guard | `us3-consumer-closeout-evidence.md`, `apps/hmi-app/config/gateway-launch.json` | `Verified` |
| `FR-009` Give every shadow implementation a single disposition | `T019`-`T025`, `T030`-`T033` | boundary script, review supplement audit, targeted file review | `us2-owner-closure-evidence.md`, `us3-consumer-closeout-evidence.md` | `Verified` |
| `FR-010` Compatibility shells must have exit conditions | `T007`, `T018`-`T024`, `T031` | wave-contract review, HMI host cutover review | `remediation-wave-contract.md`, `us2-owner-closure-evidence.md`, `us3-consumer-closeout-evidence.md` | `Verified` |
| `FR-011` Consumers may only use canonical public surfaces | `T005`, `T025`, `T028`-`T032` | boundary script, targeted pytest, consumer file review | `consumer-access-contract.md`, `us3-consumer-closeout-evidence.md` | `Verified` |
| `FR-012` Docs/build/tests/inventory must align to the same boundaries | `T024`, `T032`, `T034`, `T036`, `T038` | root-entry summary, docs inventory review | `docs/README.md`, `docs/process-model/reviews/README.md`, `tests/reports/arch-203/arch-203-root-entry-summary.md` | `Verified` |
| `FR-013` Use corrected `dispense-packaging` fact attribution | `T004`, `T023`, `T033` | recheck comparison, review supplement audit | `docs/process-model/reviews/architecture-review-recheck-report-20260331-082102.md`, `docs/process-model/reviews/dispense-packaging-module-architecture-review-20260331-074840.md` | `Verified` |
| `FR-014` Every module must have minimum acceptance evidence | `T001`-`T003`, `T014`, `T026`, `T035`, `T038` | evidence completeness review | `traceability-matrix.md`, `us1-owner-map-evidence.md`, `us2-owner-closure-evidence.md`, `us3-consumer-closeout-evidence.md` | `Verified` |
| `FR-015` Enforce a shared remediation sequence | `T007`, `tasks.md` | quickstart review, wave-contract review | `quickstart.md`, `remediation-wave-contract.md`, `tasks.md` | `Verified` |
| `FR-016` Each module must be assigned to a wave and priority | `plan.md`, `tasks.md`, `T007` | planning review | `plan.md`, `remediation-wave-contract.md`, `tasks.md` | `Verified` |
| `FR-017` Doc-only closure is not accepted as done | `T006`, `T027`, `T036`, `T038` | boundary script, root-entry summary, local gate summary | `tests/reports/module-boundary-bridges-current/module-boundary-bridges.json`, `tests/reports/arch-203/arch-203-root-entry-summary.md`, `tests/reports/local-validation-gate/arch-203-blocked-summary.md` | `Verified` |
| `FR-018` Exclude unrelated performance/UI/open-ended redesign | scope freeze only; no task may introduce unrelated feature work | diff review, scope review | `spec.md`, `plan.md`, `tasks.md` | `Active constraint` |

## Story Evidence Ledger

| Story | Primary Evidence File | Current Status | Next Update Trigger |
| --- | --- | --- | --- |
| `US1` | `us1-owner-map-evidence.md` | `Verified` | 仅在 owner-map 规则再次变更时重开 |
| `US2` | `us2-owner-closure-evidence.md` | `Verified` | 仅在 owner seam 再次变更时重开 |
| `US3` | `us3-consumer-closeout-evidence.md` | `Verified` | 仅在消费者接入面再次变更时重开 |

## Open Items

1. none
