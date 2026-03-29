# Contract: Trigger Authority and Segment-Anchored Distribution

## 1. 目的

定义 planning owner chain 在本特性中必须满足的内部 authority contract：谁负责生成权威出胶点、如何按几何线段做首尾双锚定、何时判定短线段例外，以及哪些旧 fallback 必须明确失败。

## 2. Owner Surface

- `D:\Projects\SiligenSuite\modules\dispense-packaging\application\services\dispensing\DispensePlanningFacade.cpp`
- `D:\Projects\SiligenSuite\modules\workflow\domain\domain\dispensing\planning\domain-services\DispensingPlannerService.cpp`
- 允许复用 `D:\Projects\SiligenSuite\shared\kernel\include\shared\types\TrajectoryTriggerUtils.h` 中的 marker utilities

## 3. 输入

| 输入 | 说明 |
|---|---|
| `process_path` | 包含 `dispense_on` 语义的真实几何路径 |
| `motion_plan` | 当前执行计划对应的 motion 结果 |
| `target_spacing_mm` | 固定为 `3.0 mm` |
| `spacing_window_mm` | 固定为 `2.7-3.3 mm` |
| `use_interpolation_planner` | 若为 `true`，必须生成可写回显式 trigger 的 `interpolation_points` |

## 4. 输出

| 输出 | 约束 |
|---|---|
| `interpolation_points` | 权威轨迹点列；若参与 preview/execution authority，必须写回显式 `enable_position_trigger` |
| `trigger_authority_points` | 由 `interpolation_points` 上的显式 trigger 派生，不可与之冲突 |
| `glue_points` | 直接来自权威 trigger positions 的 preview 主结果 |
| `derived_trigger_distances_mm` | 允许保留，但仅是派生兼容产物 |
| `spacing_validation_groups` | 必须按真实几何分组构建 |

## 5. 生成规则

1. 每条 `dispense_on` 几何线段必须先固定首点和尾点。
2. 对线段长度 `L`，分段数取 `n = max(1, round(L / 3.0))`。
3. 该线段上的权威锚点为 `n + 1` 个，包含首尾两点。
4. 若共享顶点被多条线段共同使用，owner chain 必须去重，只保留一个公共坐标。
5. 生成后的显式 trigger 必须写回权威 `interpolation_points`，而不是只构造单独 preview 点集。

## 6. 例外与复核规则

1. 如果线段在首尾双锚定前提下，均匀分布得到的实际间距落出 `2.7-3.3 mm`，该线段被判定为短线段例外。
2. 短线段例外仍必须保留首尾锚点。
3. spacing 复核必须按真实几何分组进行，不能把全部 `glue_points` 扁平拼接后统一评估。

## 7. 禁止行为

以下行为在本特性中一律禁止：

1. 预览 authority 回退到 `motion_trajectory_points`
2. 在显式 trigger 缺失时，从 `trigger_distances_mm` 重新构造 preview 主结果
3. 在显式 trigger 缺失时，退化为固定间隔或定时采样
4. 自动转换旧结果或历史结果以假装满足当前 authority contract
5. 通过 HMI 或 gateway 的末端重采样来修复 planning owner 的几何问题

## 8. 失败条件

出现以下任一情况时，owner chain 必须失败并把失败显式传播到 preview gate：

1. 无法生成可写回显式 trigger 的权威 `interpolation_points`
2. 显式 trigger 数量、位置或顺序与应出胶几何不一致
3. 共享顶点无法收敛为单一公共坐标
4. 需要的 authority 结果只能通过旧 fallback 才能获得
