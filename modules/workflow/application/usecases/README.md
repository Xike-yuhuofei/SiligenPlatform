# usecases

本目录是 workflow 历史 `usecases/` 轴的残留实现面，不是 `M0 workflow` 的 canonical application public surface。

## 当前状态

- 当前物理子目录仍包括 `dispensing/`、`motion/`、`system/`、`redundancy/`。
- 其中只有与 workflow 编排直接相关的最小 orchestration 逻辑允许继续存在；其余 concrete 只能缩减或外迁，不能继续扩张。
- recipe family 已从本目录退出，canonical owner 位于 `modules/recipe-lifecycle`。
- 事件发布契约统一从 `runtime/contracts/system/IEventPublisherPort.h` 引入，不再通过 workflow 旧 `domain/system/*` 或其它 compat 路径表达。

## 约束

- 不允许把本目录重新包装成 `workflow` 的稳定 public API。
- 不允许新增 wrapper、compat header、bridge facade 或“先过编译”的中间层。
- motion / system / dispensing concrete 若不属于 workflow orchestration，必须继续向 owner 模块迁出，而不是继续沉积在这里。
