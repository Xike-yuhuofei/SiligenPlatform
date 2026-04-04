# Stage D Traceability Matrix

| Requirement | Implementation | Verification | Status |
| --- | --- | --- | --- |
| workflow 不再保留 planning owner concrete | 删除 `modules/workflow/domain/domain/dispensing/planning/**` | boundary script + owner boundary test | Done |
| workflow 不再保留 planning owner public header | 删除 `modules/workflow/domain/include/domain/dispensing/planning/**` | boundary script + owner boundary test | Done |
| preview assembly compat 壳退场 | 删除 `PlanningPreviewAssemblyService.*` | boundary script + repo scan | Done |
| canonical planning owner 仍位于 dispense-packaging | 保留 `dispense-packaging` domain/application planning surface | unit tests | Done |
