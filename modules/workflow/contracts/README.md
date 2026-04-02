# workflow contracts

`modules/workflow/contracts/` 承载 `M0 workflow` 的模块 owner 专属契约。

## 契约范围

- 工作流阶段状态与编排指令的语义约束
- `M0 -> M4` 规划链触发所需的最小输入/输出口径
- 调度失败归类与恢复策略所需契约字段

## 当前已落地契约

- `WorkflowStageState`
- `WorkflowCommand`
- `WorkflowPlanningTriggerRequest`
- `WorkflowPlanningTriggerResponse`
- `WorkflowFailureCategory`
- `WorkflowRecoveryDirective`

## 边界约束

- 仅放置 `workflow` owner 专属契约，不放跨模块公共稳定契约。
- 跨模块长期稳定契约应维护在 `shared/contracts/`。
- 历史 `process-runtime-core/` 与 `src/` bridge root 已退役，不再承载 `M0` 终态 owner 契约。
- 契约字段仅表达编排、阶段推进、失败分类与恢复指令，不承载 motion / recipe / planning 的内部算法参数或状态机细节。

## 迁移来源（当前事实）

- `legacy-archive/process-runtime-core/src/application`
