# WF-T16 Engineering Tools Owner Trace

- Task type: blocked-trace
- Target residues: `WF-R031`
- Depends on: none
- Allowed scope: `modules/workflow/application/engineering_preview_tools/**`; `modules/workflow/application/engineering_trajectory_tools/**`; `modules/workflow/application/engineering_simulation_tools/**`
- Forbidden scope: implementation edits; unrelated workflow cleanup

## Prompt

```text
你负责 `WF-T16 Engineering Tools Owner Trace`。
目标 residue：`WF-R031`。
这不是实现任务，而是 owner trace 任务。只确认这些 offline engineering Python tools 的真实入口 owner、调用链和 consumer，判断它们是否真的不该留在 `workflow/application`。
禁止事项：不要直接搬迁工具目录；不要用文件语义猜测替代调用链证据。
完成标准：给出 owner freeze 结论、consumer 清单、是否能进入后续执行 batch 的 readiness 结论。
验证：入口扫描、调用链梳理、artifact ownership 判断。
停止条件：若缺少足够入口证据，明确标记为 `needs-trace`，不要假关闭。
输出：证据链、owner 结论、若转正则建议并入的 batch。
```
