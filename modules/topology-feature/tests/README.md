# tests

topology-feature 的模块级验证入口应收敛到此目录。

当前最小正式测试面：

- `unit/`
  - `ContourAugmentConfig` 默认值冻结
- `contract/`
  - canonical contract header 公开面一致性
- `golden/`
  - `ContourAugmentConfig` 默认快照 golden
- `integration/`
  - `ContourAugmenterAdapter::ConvertFile` 最小错误面集成回归

当前未补：

- 更强的几何拓扑产物 golden
- 更完整的上游链 `job-ingest -> dxf-geometry -> topology-feature` integration
- `normal / boundary` 级样本

当前 skeleton：

- `regression/`
  - 当前仅保留 `.gitkeep`，用于维持 `S06` 推荐骨架，不作为 live 测试收敛面。
