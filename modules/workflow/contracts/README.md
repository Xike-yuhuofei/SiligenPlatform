# workflow contracts

`modules/workflow/contracts/` 承载 `M0 workflow` 的模块 owner 专属稳定契约。

## 契约范围

- 工作流 command / query / event 的稳定消息面
- `WorkflowRun / StageTransitionRecord / RollbackDecision` 的查询 DTO
- 顶层失败分类、恢复建议与回退目标的稳定枚举

## 当前目录

- `commands/`
- `queries/`
- `events/`
- `dto/`
- `failures/`
- `WorkflowContracts.h` 只做聚合 `#include`

## 边界约束

- 仅放置 `workflow` owner 专属契约，不放 planning / execution specific request-response。
- 跨模块长期稳定契约应维护在 `shared/contracts/`。
- `modules/workflow/contracts/` 当前没有额外 compat / bridge 子树；live 契约只存在于 `include/workflow/contracts/**`。
- 契约字段仅表达编排、阶段推进、失败分类、回退与归档握手，不承载 motion / planning 的内部算法参数或状态机细节。

## 明确不属于本目录的对象

- `WorkflowPlanningTriggerRequest`
- `WorkflowPlanningTriggerResponse`
- 任意 `M1-M10` 的 domain / application concrete
