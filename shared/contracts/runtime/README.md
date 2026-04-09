# runtime-contracts

`shared/contracts/runtime/` 承接跨 `workflow`、`runtime-execution`、`apps/runtime-service` 共享的稳定运行期契约。

## 当前范围

- `runtime/contracts/system/IEventPublisherPort.h`

## 边界约束

- 这里只放跨模块稳定契约，不放 runtime-execution 私有执行契约或具体 adapter。
- `workflow`、`runtime-execution`、`apps/*` 不得再把事件发布契约物理落在各自模块内。
