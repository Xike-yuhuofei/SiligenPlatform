# Device Boundary Split

> Historical migration note: 本文保留 legacy `control-core` 拆边界时的阻塞快照；若出现已退役历史管理路径，仅表示历史 residual，不代表当前主线能力。

## Contracts

`packages/device-contracts` owns:

- motion / dispensing / digital-io / connection commands
- device session / motion / digital-io / dispenser / health state
- axis / io / trigger / dispenser capability models
- device fault models
- device events
- runtime-facing device ports

Rules:

- only depends on `shared-kernel`
- does not depend on `device-adapters`
- does not depend on `process-runtime-core`

## Adapters

`packages/device-adapters` owns:

- MultiCard control-card wrappers
- digital IO adapters
- vendor SDK binaries and wrapper headers
- fake motion / fake dispenser devices
- hardware error mapping

Rules:

- depends on `device-contracts` and `shared-kernel`
- must not depend on `process-runtime-core`
- must not host business state machines

## Compatibility

`control-core/modules/device-hal` is now a compatibility shell:

- public headers point to canonical contracts/adapters
- `siligen_device_hal` is an interface target
- the real implementation target is `siligen_device_adapters`

## Remaining Illegal Legacy Coupling

These still exist under `control-core/modules/device-hal/src` and are intentionally listed for follow-up:

- motion-domain types and ports mixed into motion adapters
- dispensing/configuration domain types mixed into valve and trigger adapters
- retired file-persistence repositories still live under `device-hal/src/adapters/recipes`
- diagnostics logging still lives under `device-hal/src/adapters/diagnostics/logging`

## Next Split Points

- split `MotionDevicePort` into connection / axis / homing / interpolation ports
- split digital IO into input / output / trigger ports
- move retired file persistence out of `device-hal`
- move diagnostics logging/query concerns out of `device-hal`
