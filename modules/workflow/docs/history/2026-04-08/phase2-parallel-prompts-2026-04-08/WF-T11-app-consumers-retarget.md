# WF-T11 App Consumers Retarget

- Historical note: 本文件保留 `2026-04-08` 的执行提示快照，仅供追溯。
- Historical note: 下文若出现已退役历史管理面的旧名称或旧路径，请按历史执行语境理解。

- Task type: parallel-worker
- Target residues: `WF-R024`
- Depends on: Wave B outputs merged by `WF-T00`
- Allowed scope: `apps/planner-cli/**`; `apps/runtime-service/**`; `apps/runtime-gateway/transport-gateway/**`
- Forbidden scope: `modules/workflow/**`; `modules/runtime-execution/**`

## Prompt

```text
你负责 `WF-T11 App Consumers Retarget`。
目标 residue：`WF-R024`。
只处理 app 侧 consumer retarget：`planner-cli`、`runtime-service`、`runtime-gateway` 不再通过 `application/usecases/*` 或 `workflow/application/usecases/recipes/*` 获取 workflow / foreign owner 能力。
你不是集成器，不得修改 `modules/workflow/**` 或 `modules/runtime-execution/**`。
禁止事项：不要顺手在 workflow 内新增 wrapper 头来兼容旧 include；不要修改 tests 或 gate。
完成标准：`WF-R024` 的 acceptance 可机械通过。
验证：include path check；targeted build；`rg` 确认 apps 不再包含旧 workflow usecase include 根。
停止条件：若缺少 canonical public surface 或必须回改 workflow shared files，停止并把挂点交给 `WF-T00`。
输出：修改文件清单、验证结果、仍缺失的 canonical public surface 清单。
```
