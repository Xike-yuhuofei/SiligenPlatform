# application

`modules/dispense-packaging/application/` 是 `M8` 对外暴露 application consumer surface 的当前入口。

- `AuthorityPreviewAssemblyService` 当前承担 authority preview payload 组装。
- `ExecutionAssemblyService` 当前承担 `ExecutionPackageValidated` 与 execution binding 组装。
- `PlanningArtifactExportRequest` 已迁回 `workflow` owner contract，本目录不再组织 live export request。
- 规划算法实现不得回流到本目录；上游路径/轨迹规划分别由 `M6`、`M7` owner 提供。
- 第一阶段不在本目录执行 owner 迁移，只修正 current-fact 文档和边界说明。
