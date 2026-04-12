# recipe-lifecycle

`modules/recipe-lifecycle/` 是仓库内 recipe 生命周期能力的 canonical owner 入口。
按 `docs/architecture/dsp-e2e-spec/project-chain-standard-v1.md` 的口径，它对应
`B08 配方生命周期链`，而不是 `S05` 正式 `M0-M11` 一级模块表中的新增 owner。

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

## 与冻结架构的关系

- `架构真值`
  - `S05` 正式一级模块仍只有 `M0-M11`；本模块不应被表述成新的正式 `M` 级 owner。
  - `project-chain-standard-v1.md` 里已经冻结了 `B08 配方生命周期链`，说明仓库内确实存在独立的 recipe 生命周期能力链。
- `当前实现事实`
  - 当前仓库已经存在独立的 `modules/recipe-lifecycle/{domain,application,adapters,tests}` 构建与源码面。
  - `module.yaml` 使用 `module_id: B08`，而模块 `CMakeLists.txt` 没有声明 `SILIGEN_MODULE_ID`。
- `本轮口径`
  - 只把它当作仓库内的局部 canonical owner / 辅助应用模块治理。
  - 不借本轮目录整理把它升格为新的正式一级 owner 事实源。
