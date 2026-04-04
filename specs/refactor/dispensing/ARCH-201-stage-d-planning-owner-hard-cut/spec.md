# Feature Spec: ARCH-201 Stage D - Planning Owner Hard Cut

## Goal

将 planning owner 最终收敛到 `modules/dispense-packaging/`，让 `workflow` 只保留编排、缓存、导出调度与失败传播，不再保留 planning concrete、public planning owner header 或 preview assembly compat 壳。

## Success Criteria

- `workflow` 仓库路径下不再保留 planning owner concrete 或 public owner header。
- `PlanningUseCase` 继续通过 `AuthorityPreviewAssemblyService` / `ExecutionAssemblyService` 完成 authority preview 与 execution assembly。
- 边界脚本与 owner boundary tests 都能对 workflow planning residual 做硬失败。
- 不修改 HMI、gateway、runtime-service 对外协议。

## Non-Goals

- 不重写 `PlanningUseCase` 的请求/响应结构。
- 不在本阶段重做 `dispense-packaging` 内部 planner 算法。
- 不顺手清理与 planning owner seam 无关的 dispense residual。
