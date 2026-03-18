# device-hal

`device-hal` 现在是 legacy 兼容壳，不再是 `device-contracts` / `device-adapters` 的真实依赖来源。

当前职责：

- `include/siligen/hal/*` 继续提供兼容公开头，并转到 canonical contracts/adapters
- `siligen_device_hal` 继续作为 interface target 暴露给 legacy consumer

已经切出的真实 owner：

- `packages/device-contracts`
- `packages/device-adapters`

仍留在 legacy 目录、阻止整体删除的实现：

- `src/adapters/recipes/*`
- `src/adapters/diagnostics/logging/*`
- `src/adapters/diagnostics/health/*`
- `src/adapters/motion/controller/*`
- `src/adapters/dispensing/dispenser/*`
- `src/drivers/multicard/MultiCardAdapter.*`
- `src/drivers/multicard/MultiCardErrorCodes.*`
- `src/drivers/multicard/PositionTriggerController.cpp`
