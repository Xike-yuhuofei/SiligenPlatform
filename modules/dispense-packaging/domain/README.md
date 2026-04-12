# domain

`modules/dispense-packaging/domain/` 当前只有 `domain/dispensing/` 承载 source-bearing
代码。

本目录需要区分两层事实：

- `架构真值`
  - `M8` 正式 owner 是 `DispenseTimingPlan`、`ExecutionPackage` 等执行准备事实。
  - `M8` 不应回流为 runtime owner，也不应重新拥有路径/轨迹规划事实。
- `当前实现事实`
  - `domain/dispensing/` 里仍混有 planning/execution residual。
  - 本轮不把这些 residual 伪装成 canonical owner surface，只做目录语义澄清。
