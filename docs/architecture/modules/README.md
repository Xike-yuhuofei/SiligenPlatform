# 模块边界索引

本目录把 `docs/architecture/dsp-e2e-spec/` 的冻结规格映射到 `modules/` 当前实施面，作为模块级阅读入口。

统一约束：

- owner、阶段、对象、命令、事件以 `S01/S04/S05/S07/S09` 为准。
- `apps/` 只能消费模块公开契约，不承接 owner 实现。
- 兼容桥只允许存在于显式 bridge target 中，不允许散落直连。

模块入口：

- [workflow](./workflow.md)
- [workflow target-state v1](./workflow-target-state-v1.md)
- [workflow target-state mapping v1](./workflow-target-state-mapping-v1.md)
- [workflow target-state task breakdown v1](./workflow-target-state-task-breakdown-v1.md)
- [job-ingest](./job-ingest.md)
- [dxf-geometry](./dxf-geometry.md)
- [topology-feature](./topology-feature.md)
- [process-planning](./process-planning.md)
- [coordinate-alignment](./coordinate-alignment.md)
- [process-path](./process-path.md)
- [motion-planning](./motion-planning.md)
- [dispense-packaging](./dispense-packaging.md)
- [runtime-execution](./runtime-execution.md)
- [trace-diagnostics](./trace-diagnostics.md)
- [hmi-application](./hmi-application.md)
