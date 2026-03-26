# process-planning contracts

`modules/process-planning/contracts/` 承载 `M4 process-planning` 的模块 owner 专属契约。

## 契约范围

- `ProcessPlan` 对象语义与版本口径。
- 工艺规划命令输入、校验与输出事件约定。
- `M4 -> M5/M6/M8` 交接对象的最小字段边界（`CoordinateTransformSet`、`ProcessPath`、`DispenseTimingPlan`）。

## 边界约束

- 仅放置 `M4 process-planning` owner 专属契约，不放跨模块稳定公共契约。
- 不承载运行时执行层与 HMI 展示层语义。
- `modules/process-planning/` 与 `shared/contracts/engineering/` 构成当前 canonical 契约承载面。
