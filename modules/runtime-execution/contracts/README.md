# runtime-execution contracts

`modules/runtime-execution/contracts/` 承载 `M9 runtime-execution` 的模块 owner 专属契约。

## 契约范围

- 运行态控制命令、执行状态与故障事件的语义约束。
- `runtime-service`/`runtime-gateway` 与运行时执行核心之间的最小输入输出口径。
- 设备适配层交接所需的命令、回执与异常分类字段。

## 边界约束

- 仅放置 `M9 runtime-execution` owner 专属契约，不放跨模块长期稳定公共契约。
- 跨模块稳定设备契约应维护在 `shared/contracts/device/`。
- `modules/runtime-execution/contracts/device/`、`modules/runtime-execution/runtime/host/`、`modules/runtime-execution/adapters/device/` 是当前 canonical 契约与消费面。
