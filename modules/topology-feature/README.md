# topology-feature

目标 owner：`M3 topology-feature`

当前正式事实来源：`modules/topology-feature/adapters/infrastructure/adapters/planning/geometry/`

owner 入口：
- 构建入口：`modules/topology-feature/CMakeLists.txt`（target：`siligen_module_topology_feature`）
- 契约入口：`modules/topology-feature/contracts/README.md`
- 测试入口：`modules/topology-feature/tests/CMakeLists.txt`

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- `adapters/infrastructure/adapters/planning/geometry/` 已成为当前唯一真实实现承载面，模块根 target 直接链接 canonical adapter target。
- `contracts/include/topology_feature/contracts/` 是当前唯一公共契约入口。
- 所有 live 实现、构建入口与公共 surface 都已收敛到 canonical roots。

## 当前测试面

- `tests/unit/`
  - 冻结 `ContourAugmentConfig` 默认值
- `tests/contract/`
  - 冻结 canonical contract header 的公开面
- `tests/golden/`
  - 冻结默认配置快照 golden
- `tests/integration/`
  - 冻结 `ConvertFile` 的最小错误面与文件输出行为
