# topology-feature

目标 owner：`M3 topology-feature`

当前正式事实来源：`modules/topology-feature/domain/geometry/`

owner 入口：
- 构建入口：`modules/topology-feature/CMakeLists.txt`（target：`siligen_module_topology_feature`）
- 契约入口：`modules/topology-feature/contracts/README.md`

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- `domain/geometry/` 已成为当前唯一真实实现承载面，模块根 target 直接链接 canonical 子域 target。
- 所有 live 实现与构建入口都已收敛到 canonical roots。

