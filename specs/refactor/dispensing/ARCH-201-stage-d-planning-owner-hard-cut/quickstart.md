# Quickstart

1. 在 `D:/Projects/wt-arch201d` 工作，不回到 Stage C worktree 继续叠加改动。
2. 任何新增 planning 语义必须落到 `dispense-packaging` owner 路径。
3. `workflow` 只允许保留 orchestrator 代码、cache、export port、response mapping。
4. `PlanningUseCase` 的 live seam 固定为 `ProcessPathFacade + MotionPlanningFacade + AuthorityPreviewAssemblyService + ExecutionAssemblyService`；不要把 `DispensePlanningFacade` 回流成 workflow direct seam。
5. 当前 docs/index 不允许再把 removed workflow planning artifacts 当作 live 文件树或 live owner 证据；历史评审/旧 spec 可保留为证据。
6. 提交前必须通过边界脚本和 Stage D 收口定向测试。
