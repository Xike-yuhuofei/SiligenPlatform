# WF-T14 Layout Validator Reverse Dependency Trace

- Task type: blocked-trace
- Target residues: `WF-R029`
- Depends on: none
- Allowed scope: `scripts/migration/validate_workspace_layout.py`; trace notes only
- Forbidden scope: code cleanup implementation; `modules/workflow/**` behavior edits

## Prompt

```text
你负责 `WF-T14 Layout Validator Reverse Dependency Trace`。
目标 residue：`WF-R029`。
这不是实现任务，而是 trace 任务。只确认 `scripts/migration/validate_workspace_layout.py` 是否仍把 workflow 当 foreign owner wiring hub，或是否仍为历史 reverse mutation 保留结构性前提。
禁止事项：不要顺手修改 workflow / runtime / apps 代码；不要在未证实前把 suspected 当 confirmed。
完成标准：给出明确结论：`ready`、`needs-trace` 或 `not-a-residue`，并附具体 matcher、路径、规则证据。
验证：脚本规则比对；必要时最小 dry-run 或静态 matcher 梳理。
停止条件：若必须执行 CI gate 之外的大范围改动，停止并回报。
输出：证据链、结论、若转正则建议并入的后续 batch / task。
```
