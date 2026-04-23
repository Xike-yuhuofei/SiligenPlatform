# Workflow Phase 2 Parallel Prompt Pack

- Historical note: 本目录保留 `2026-04-08` 的并行执行提示快照，仅供追溯，不应直接替代当前代码真值。
- Historical note: 下文若出现已退役历史管理面的旧名称或旧路径，请按历史执行语境理解。

- Baseline: `modules/workflow/docs/workflow-residue-ledger-2026-04-08.md`
- Goal: provide one independent prompt document per execution task for the `modules/workflow` legacy/compat/bridge clean-exit phase 2
- Rule: `WF-T00` is the only task allowed to edit shared aggregator files
- Shared single-writer files:
- `CMakeLists.txt` (repo root)
- `modules/workflow/CMakeLists.txt`
- `modules/workflow/domain/CMakeLists.txt`
- `modules/workflow/domain/domain/CMakeLists.txt`
- `modules/workflow/application/CMakeLists.txt`

## Wave Order

- Wave 0 serial: `WF-T00`
- Wave A parallel after `WF-T00`: `WF-T01`, `WF-T02`, `WF-T03`, `WF-T04`, `WF-T05`, `WF-T06`, `WF-T07`, `WF-T08`
- Wave B parallel after Wave A outputs are merged by `WF-T00`: `WF-T09`, `WF-T10`
- Wave C parallel after Wave B outputs are merged by `WF-T00`: `WF-T11`, `WF-T12`, `WF-T13`
- Deferred trace lanes: `WF-T14`, `WF-T15`, `WF-T16`, `WF-T17`
- Final serial sync after execution and traces: `WF-T18`

## Task Map

- `WF-T00`: shared graph and aggregate integrator; owns `WF-R001`, `WF-R004`, `WF-R005`, `WF-R014`, `WF-R015`, `WF-R016`, `WF-R017`
- `WF-T01`: application public surface cleanup; owns `WF-R018`, `WF-R019`
- `WF-T02`: domain public surface cleanup; owns `WF-R020`, `WF-R021`, `WF-R022`, `WF-R023`
- `WF-T03`: recipe family cleanup; owns `WF-R002`, `WF-R008`, `WF-R010`
- `WF-T04`: dispensing execution cleanup; owns `WF-R006`
- `WF-T05`: valve application cleanup; owns `WF-R011`
- `WF-T06`: machine domain cleanup; owns `WF-R007`
- `WF-T07`: system application cleanup; owns `WF-R012`
- `WF-T08`: DXF adapter cleanup; owns `WF-R026`
- `WF-T09`: motion application cleanup; owns `WF-R013`
- `WF-T10`: safety domain cleanup; owns `WF-R003`, `WF-R009`
- `WF-T11`: apps consumer retarget; owns `WF-R024`
- `WF-T12`: runtime-execution retarget; owns `WF-R025`
- `WF-T13`: workflow tests boundary cleanup; owns `WF-R027`
- `WF-T14`: layout validator reverse-dependency trace; owns `WF-R029`
- `WF-T15`: system/planning include trace; owns `WF-R030`
- `WF-T16`: engineering tools owner trace; owns `WF-R031`
- `WF-T17`: legacy exit gate coverage trace; owns `WF-R032`
- `WF-T18`: doc-contract sync; owns `WF-R028`

## Handoff Rule

- Every worker must report changed files, requested `WF-T00` shared-file changes, acceptance evidence, and unresolved blockers.
