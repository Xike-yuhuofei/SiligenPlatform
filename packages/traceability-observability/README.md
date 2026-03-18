# traceability-observability

目标定位：日志、事件、报警记录、回放、查询模型与持久化抽象。

当前真实迁移来源：

- `control-core/apps/control-runtime/runtime/events`
- `control-core/apps/control-runtime/runtime/storage`
- `control-core/src/shared/logging`
- `control-core/modules/device-hal/src/adapters/diagnostics/*`

当前阶段说明：

- 本目录先作为 canonical 占位
- 后续要把当前散落的观测能力从 `device-hal` 等目录中拆出
