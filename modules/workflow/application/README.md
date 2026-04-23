# application

workflow 的 canonical application 入口应收敛到此目录。

## 当前真值

- `commands/`、`queries/`、`facade/` 已作为 M0 终态 skeleton 落位。
- `planning-trigger/`、`phase-control/` 与 `ports/dispensing/` foreign surface 已物理删除并迁回 owner 模块。
- 原 `recovery-control/` 与 `usecases/redundancy/` 已确认为非 M0 的代码治理 residue，不再属于 workflow canonical build graph。
- recipe management CRUD / publish / import-export surface 已退役并删除；workflow application 不再承接任何 recipe management owner。
- 事件发布契约统一从 `runtime/contracts/system/IEventPublisherPort.h` 引入，其物理 owner 位于 `shared/contracts/runtime`。
- `include/workflow/application/**` 只保留 `commands/queries/facade` wrapper。
- `include/application/**` dual-root residue 与 `usecases/ / ports/ / services/ / recovery-control/` compat 目录已退出。
- `application/CMakeLists.txt` 只做 skeleton 校验，不再导出 `siligen_workflow_application_headers` 这类 bridge target。

## 禁止事项

- 不允许重新引入 recipe、system、motion runtime assembly、valve 或 engineering concrete 作为 workflow application owner。
- 不允许新增 compat / bridge / facade shell 来为 foreign owner 长期兜底。
- 不允许把 `usecases/` 目录重新包装成对外 canonical include root。
