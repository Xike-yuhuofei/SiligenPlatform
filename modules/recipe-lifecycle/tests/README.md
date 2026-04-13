# tests

`modules/recipe-lifecycle/tests/` 是当前模块内验证入口。

## 当前 source-bearing 目录

- `unit/domain/recipes/`
  - 领域服务的 owner 内聚回归。
- `unit/application/recipes/`
  - recipe use case 的应用层回归。

## 当前未补齐的分层

- `contract/`
  - 目前没有独立 source-bearing 契约测试，避免为了对齐骨架新增空壳目录。
- `integration/`
  - 目前没有模块内集成测试入口，后续只在出现真实验证需求时再补。
- `regression/`
  - 目前没有独立回归样本目录，不做占位污染。

## 说明

- 本轮把 `tests/canonical/unit` 收口为 `tests/unit/...`，只是在模块内部让目录表达更接近 `S06` 的测试分层原则。
- 这不改变 `recipe-lifecycle` 在正式冻结架构中的地位；它仍按 `B08` 辅助链路处理，而不是新的 `M` 级 owner。
