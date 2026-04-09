# application

workflow 的 canonical application 入口应收敛到此目录。

## 当前真值

- 对外 public surface 应只围绕 `planning-trigger/`、`phase-control/`、`recovery-control/` 展开。
- recipe CRUD / publish / import-export 已迁到 `modules/recipe-lifecycle`，不再属于 workflow application owner。
- 事件发布契约统一从 `runtime/contracts/system/IEventPublisherPort.h` 引入，其物理 owner 位于 `shared/contracts/runtime`。
- `usecases/` 子目录当前只应被视为迁移残留与内部实现目录，不是 workflow 终态 public surface。

## 当前 residue

- `siligen_workflow_application_headers` 仍链接部分 foreign owner headers/contracts，说明 application public surface 还没有完全收口。
- `planning-trigger` 仍需要编排 `motion-planning`、`dispense-packaging`、`runtime-execution` 等 canonical owner 模块，但这种依赖只允许停留在 orchestration 边界，不得继续回流为 workflow 自己的 concrete owner。

## 禁止事项

- 不允许重新引入 recipe、system、motion runtime assembly、valve 或 engineering concrete 作为 workflow application owner。
- 不允许新增 compat / bridge / facade shell 来为 foreign owner 长期兜底。
- 不允许把 `usecases/` 目录重新包装成对外 canonical include root。
