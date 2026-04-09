# recipe-lifecycle

`modules/recipe-lifecycle/` 是 recipe 生命周期能力的 canonical owner 入口。

## Owner 范围

- recipe / recipe version 聚合与值对象
- recipe repository / audit / template / schema / bundle serializer ports
- recipe validation / activation 领域规则
- recipe CRUD / publish / activate / compare / import / export 用例
- recipe JSON serialization canonical utility

## 边界约束

- `workflow` 不再承载 recipe domain、use case 或 serialization 实现。
- `runtime-service` 只承接 recipe repository / file storage / schema provider 等 app-local runtime wiring，不再持有 recipe 规则 owner。
- `runtime-gateway`、`planner-cli` 只消费本模块 public surface，不再经 `workflow` recipe target 透传。
