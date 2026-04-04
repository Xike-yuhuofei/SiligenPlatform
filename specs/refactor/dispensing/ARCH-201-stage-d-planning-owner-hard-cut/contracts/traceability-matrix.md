# Stage D Traceability Matrix

| Requirement | Implementation | Verification | Status |
| --- | --- | --- | --- |
| workflow 不再保留 planning owner concrete | 删除 `modules/workflow/domain/domain/dispensing/planning/**` | boundary script + owner boundary test | Done |
| workflow 不再保留 planning owner public header | 删除 `modules/workflow/domain/include/domain/dispensing/planning/**` | boundary script + owner boundary test | Done |
| preview assembly compat 壳退场 | 删除 `PlanningPreviewAssemblyService.*` | boundary script + repo scan | Done |
| canonical planning owner 仍位于 dispense-packaging | 保留 `dispense-packaging` domain/application planning surface | unit tests | Done |
| 现行模块索引不再回报 workflow planning residual | 清理 `modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md` 中的 workflow planning 文件树、实现说明与统计残留 | repo scan + doc diff | Done |
| repo scan 仅将历史评审与旧 spec 视为保留证据，不再作为当前待清理 live residual | 第二批仅纳入 current docs/index tightening，不改写历史证据文档 | repo scan | Done |
| workflow direct seam 固定为 split assembly services | `PlanningUseCase` 继续通过 `AuthorityPreviewAssemblyService` / `ExecutionAssemblyService` 编排 M8，现行基线文档不再把 `DispensePlanningFacade` 写成 direct seam | workflow tests + doc diff | Done |
| M8 split assembly public surface 具备 direct owner-side evidence | 为 `AuthorityPreviewAssemblyService` / `ExecutionAssemblyService` 增加 direct tests，并与 facade 对齐关键输出语义 | dispense-packaging unit tests | Done |
| boundary test 与 hard-cut residual 清单完全对齐 | `MotionPlanningOwnerBoundaryTest` 的 removed artifact 列表与 boundary script 1:1 一致 | motion-planning unit tests + boundary script | Done |
