# adapters

topology-feature 的外部依赖适配实现应收敛到此目录。

- `infrastructure/adapters/planning/geometry/` 是 `ContourAugmenterAdapter` 的当前 canonical live 实现目录。
- 顶层 `adapters/` 仍在当前 `SILIGEN_TARGET_IMPLEMENTATION_ROOTS` 中，因为 live 代码实际收敛在该子树。
- 对该深层路径做物理扁平化需要同步更新 public seam、文档与 consumer；本轮不实施。
