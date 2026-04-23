# WF-T12 Runtime Execution Retarget

- Task type: parallel-worker
- Target residues: `WF-R025`
- Depends on: Wave B outputs merged by `WF-T00`
- Allowed scope: `modules/runtime-execution/**`
- Forbidden scope: `modules/workflow/**`; `apps/**`

## Prompt

```text
你负责 `WF-T12 Runtime Execution Retarget`。
目标 residue：`WF-R025`。
只处理 `runtime-execution` 对 workflow compat surface 的反向依赖退出：不再链接 `siligen_domain` / `siligen_motion_core`，其 contracts 不再包含 workflow `domain/*` 头。
你不是集成器，不得修改 `modules/workflow/**` 或 apps。
禁止事项：不要通过新增 compat alias 保留旧依赖；不要顺手修 unrelated runtime 问题。
完成标准：`WF-R025` 的 acceptance 可机械通过。
验证：targeted build；link graph / include graph check；`rg` 确认 `runtime-execution` 不再引用 workflow compat target 或 workflow `domain/*` 头。
停止条件：若 canonical target 不可用或必须回改 workflow shared files，停止并把挂点交给 `WF-T00`。
输出：修改文件清单、验证结果、仍缺失的 canonical dependency 清单。
```
