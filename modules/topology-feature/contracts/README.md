# topology-feature contracts

目标 owner：`M3 topology-feature`

当前阶段定位：
- 本目录作为 `modules/topology-feature` 的契约入口。
- 契约事实与 owner 入口均以 `modules/topology-feature/` 与 `shared/contracts/engineering/` 为准。

契约对象（冻结口径）：
- `TopologyModel`
- `FeatureGraph`

当前实现事实：

- `contracts/include/topology_feature/contracts/ContourAugmentContracts.h` 当前直接 re-export `ContourAugmentConfig` 与 `ContourAugmenterAdapter`。
- 因此本目录现在是模块 public entry，但还不是纯粹映射 `TopologyModel` / `FeatureGraph` 的 owner-only contract surface。
- 若要把该 public seam 收回到真正的 owner contract，需要同步调整 consumer 与实现表达，超出本轮低成本整理边界。
