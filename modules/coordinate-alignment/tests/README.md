# tests

coordinate-alignment 的模块级验证入口应收敛到此目录。

当前最小正式测试面：

- `unit/`
  - `CalibrationProcess` 完成态与 missing-device 失败态
- `contract/`
  - `CoordinateTransformSet` 默认公开面
  - `origin-offset` / `rotation-z` 变换样本承载
- `golden/`
  - `CoordinateTransformSet` 基线样本快照

当前未补：

- 真实数值型 `origin offset / rotation` 补偿字段契约
- `coordinate-alignment -> process-path` 邻接 integration
- 历史标定回归样本与 out-of-range 覆盖
