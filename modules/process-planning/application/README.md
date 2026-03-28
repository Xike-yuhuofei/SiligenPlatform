# application

`modules/process-planning/application/` 仅暴露 configuration owner 的 consumer surface。

- canonical public surface：`include/process_planning/application/ProcessPlanningApplicationSurface.h`
- 当前只转发 `process_planning/contracts/ConfigurationContracts.h`
- 本轮不提供 `FeatureGraph -> ProcessPlan` facade、query/command handler 或完整工艺规划 use case
