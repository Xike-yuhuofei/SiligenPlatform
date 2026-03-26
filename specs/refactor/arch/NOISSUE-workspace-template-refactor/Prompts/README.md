# Prompts Workspace Index

## 用途

本目录用于当前 feature 的并行子任务分发与收口，不跨 feature 复用。

## 固定规则

1. 仅为未完成 `[P]` 任务生成 prompt。
2. `tasks.md` 仅由主线程维护。
3. 每个 Phase 都有 `Tasks-Execution-Summary.md`。
4. 子任务只允许写回自己的 slot。

## Index

| Phase | Directory | Task IDs | Prompt Count | Summary File |
|---|---|---|---:|---|
| Phase 03 | `Phase-03-user-story-1-mvp` | T014, T015, T016, T017, T018, T019 | 6 | `Tasks-Execution-Summary.md` |
| Phase 04 | `Phase-04-user-story-2-canonical-roots` | T021, T022, T023, T024, T025, T026, T027, T028, T029 | 9 | `Tasks-Execution-Summary.md` |
| Phase 05 | `Phase-05-user-story-3-m1-m3` | T031, T032, T033, T034, T035, T036, T037 | 7 | `Tasks-Execution-Summary.md` |
| Phase 06 | `Phase-06-user-story-4-m0-m4-m8` | T039, T040, T041, T042, T043, T044, T045, T046, T047 | 9 | `Tasks-Execution-Summary.md` |
| Phase 07 | `Phase-07-user-story-5-m9` | T049, T050, T051, T052, T053, T054, T055 | 7 | `Tasks-Execution-Summary.md` |
| Phase 08 | `Phase-08-user-story-6-hmi-m10-m11` | T057, T058, T059, T060, T061, T062, T063 | 7 | `Tasks-Execution-Summary.md` |
| Phase 09 | `Phase-09-user-story-7-legacy-exit` | T065, T066, T068 | 3 | `Tasks-Execution-Summary.md` |
| Phase 10 | `Phase-10-polish-cross-cutting-concerns` | T070, T071, T072, T073 | 4 | `Tasks-Execution-Summary.md` |

## Stats

- feature_dir: `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor`
- prompts_root: `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\Prompts`
- total_prompt_count: 52
- phase_summary_file: Tasks-Execution-Summary.md
- protocol_file: D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\Prompts\00-Multi-Task-Session-Summary-Protocol.md

## Main Thread Phase Flow

1. 只分发当前 Phase 的 `T###.md`。
2. 等待当前 Phase slot 收口。
3. 等待用户明确触发 Phase End Check。
4. 完成主线程收口后进入下一 Phase。
