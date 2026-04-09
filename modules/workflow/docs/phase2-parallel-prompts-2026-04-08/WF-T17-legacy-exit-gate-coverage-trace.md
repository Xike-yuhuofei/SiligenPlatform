# WF-T17 Legacy Exit Gate Coverage Trace

- Task type: blocked-trace
- Target residues: `WF-R032`
- Depends on: none
- Allowed scope: `legacy-exit-check.ps1`; `ci.ps1`; `scripts/migration/legacy_fact_catalog.json`; trace notes only
- Forbidden scope: implementation cleanup in workflow / apps / runtime

## Prompt

```text
你负责 `WF-T17 Legacy Exit Gate Coverage Trace`。
目标 residue：`WF-R032`。
这不是实现任务，而是 gate coverage trace 任务。只确认 legacy-exit / CI gate 是否真的无法阻断 `siligen_domain`、`siligen_process_core`、`siligen_motion_core`、`siligen_workflow_*_public` 这类 target-level residue。
禁止事项：不要在未执行最小证据比对前直接判定 gate 失效；不要顺手修改 workflow build graph。
完成标准：给出 gate coverage 结论、缺失 matcher 清单、是否可转为 `ready` 执行任务。
验证：规则比对、最小 gate 证据、target-level matcher 梳理。
停止条件：若需要大范围 CI 改造才能判断，停止并回报。
输出：证据链、结论、若转正则建议并入的后续 batch。
```
