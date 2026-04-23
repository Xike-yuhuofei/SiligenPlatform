# domain

workflow 的 owner 事实、值对象、不变量与少量 orchestration boundary types 应收敛到此目录。

## 当前真值

- `domain/include/workflow/domain/**` 是 workflow 唯一保留的 domain public root，承载 `WorkflowRun / StageTransitionRecord / RollbackDecision / policies / ports`。
- workflow domain 当前只保留 canonical `siligen_workflow_domain_headers` / `siligen_workflow_domain_public`；`siligen_domain` 已于 round `18` 删除。
- 已退役管理面、diagnostics sink、machine / motion / safety / dispensing concrete 已全部退出 workflow domain。
- `domain/domain/**`、`domain/include/domain/**` 与 `motion-core/**` 已物理删除，不再作为 compat root 或 dormant shell 保留。

## 禁止事项

- 不允许继续把 machine、已退役管理面、safety、motion execution concrete 回流到本目录。
- 不允许新增 sibling source bridge、compat public root 或新的 fake domain/compat target。
