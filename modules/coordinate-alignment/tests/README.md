# tests

coordinate-alignment 的模块级验证入口应收敛到此目录。

当前最小正式测试面：

- `contract/`
  - `CoordinateTransformSet` 默认公开面
  - `origin-offset` / `rotation-z` 变换样本承载
- `golden/`
  - `CoordinateTransformSet` 基线样本快照

当前不再作为 `M5` root closeout 主证据：

- `CalibrationProcess` residual family
  - 原因：它证明的是 machine/calibration residual，而不是 `CoordinateTransformSet` owner surface。
  - 处理：已退出 live build，不再保留于本模块测试面。

当前未补：

- 真实数值型 `origin offset / rotation` 补偿字段契约
- `coordinate-alignment -> process-path` 邻接 integration
- out-of-range / 数值语义覆盖

当前 skeleton：

- `unit`
- `integration`
- `regression`
  以上目录当前仅保留 `.gitkeep`，用于维持 `S06` 推荐骨架，不作为 live owner 证明。
