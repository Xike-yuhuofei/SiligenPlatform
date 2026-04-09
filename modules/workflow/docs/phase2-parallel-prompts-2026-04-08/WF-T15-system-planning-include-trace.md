# WF-T15 System Planning Include Trace

- Task type: blocked-trace
- Target residues: `WF-R030`
- Depends on: none
- Allowed scope: repo include graph trace for `modules/workflow/domain/domain/system/**` and `modules/workflow/domain/domain/planning/**`
- Forbidden scope: implementation edits; unrelated residue cleanup

## Prompt

```text
你负责 `WF-T15 System Planning Include Trace`。
目标 residue：`WF-R030`。
这不是实现任务，而是 include graph trace 任务。只确认 repo 内是否仍存在通过旧 `domain/system/*` 或 `domain/planning/*` 获取 live 语义的真实消费者。
禁止事项：不要顺手改 canonical headers；不要把仅有的两个已知 alias 点外推成全量结论。
完成标准：产出 consumer 清单、路径清单、是否足以把 `WF-R030` 变成 `ready` 的结论。
验证：repo 级 include graph 扫描；consumer 分类；直接证据与反证。
停止条件：若必须直接动代码才能继续，停止并回报。
输出：trace 结果、consumer 分类、后续建议 batch。
```
