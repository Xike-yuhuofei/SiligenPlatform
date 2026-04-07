# workflow integration tests

本目录是 `workflow` 模块 integration 承载面的正式入口。

## 当前登记目标

- `workflow_integration_motion_runtime_assembly_smoke`
  - 复用 `canonical/` 中的稳定装配测试源码。
  - 验证 canonical `workflow` application/domain/adapters 组合后，motion runtime assembly 可完成最小装配与路径执行烟测。

## 约束

- 本目录只负责 integration 级构建入口、命名与登记。
- 测试源码优先复用 `tests/canonical/` 中已稳定的 canonical 资产。
- 后续若新增更贴近 workflow facade 的集成测试，应继续落在本目录注册，不得回写 bridge 测试入口。
