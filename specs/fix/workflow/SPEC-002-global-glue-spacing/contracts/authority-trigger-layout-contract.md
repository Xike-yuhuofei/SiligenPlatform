# Contract: Authority Trigger Layout

## 1. 目的

定义 planning owner chain 在本特性中必须满足的内部 authority contract：谁负责生成权威胶点布局、如何对连续路径做全局布点、闭环如何选相位、曲线如何落到真实可见路径，以及如何把同一批胶点结果绑定到 preview 与 execution。

## 2. Owner Surface

- `D:\Projects\SiligenSuite\modules\dispense-packaging\domain\dispensing\planning\domain-services\AuthorityTriggerLayoutPlanner.h/.cpp`
- `D:\Projects\SiligenSuite\modules\dispense-packaging\domain\dispensing\planning\domain-services\PathArcLengthLocator.h/.cpp`
- `D:\Projects\SiligenSuite\modules\dispense-packaging\domain\dispensing\planning\domain-services\CurveFlatteningService.h/.cpp`
- `D:\Projects\SiligenSuite\modules\dispense-packaging\application\services\dispensing\DispensePlanningFacade.cpp`
- `D:\Projects\SiligenSuite\modules\dispense-packaging\domain\dispensing\planning\domain-services\DispensingPlannerService.cpp`

## 3. 输入

| 输入 | 说明 |
|---|---|
| `process_path` | 含 `dispense_on` 语义的真实工艺路径 |
| `motion_plan` / `interpolation config` | 当前执行计划与插补规划参数 |
| `target_spacing_mm` | 默认 `3.0 mm` |
| `spacing_window_mm` | 默认 `2.7-3.3 mm` |
| `spline_max_error_mm` | 曲线扁平化或定位容差 |
| `strong anchors` | 业务上必须保留的边界点或关键点，可为空 |

## 4. 输出

| 输出 | 约束 |
|---|---|
| `AuthorityTriggerLayout` | 当前计划唯一权威胶点布局 |
| `LayoutTriggerPoint[]` | 直接作为 preview `glue_points` 的来源 |
| `InterpolationTriggerBinding[]` | 把同一批 authority points 绑定到 execution carrier |
| `SpacingValidationOutcome[]` | 按真实连续路径分组的间距复核结论 |
| `derived_trigger_distances_mm` | 可保留为派生导出结果，但不是 preview truth |

## 5. 生成规则

1. authority layout 必须按连续 `dispense_on` span 计算，而不是按内部 segment 独立布点。
2. 对开放 span，布局必须保留业务强锚点并沿整条路径均匀求解。
3. 对闭合 span，在无业务强锚点时必须通过候选相位搜索选择稳定布局，不得固定从几何起点开始。
4. 布局距离必须落到最终可见路径上；line / arc 可直接解析，spline 必须先扁平化成误差受控路径再参与布局。
5. authority layout 生成后，preview 必须直接消费 `LayoutTriggerPoint.position`；execution 只能通过 `InterpolationTriggerBinding` 绑定同一批点，不能自行重建另一批点。

## 6. 校验与门禁规则

1. 间距复核必须按真实连续路径分组，不得跨不相连轮廓混算。
2. 校验结果必须区分 `pass`、`pass_with_exception`、`fail`。
3. `pass_with_exception` 允许继续执行，但必须带明确原因。
4. `fail`、authority 缺失、binding 失败或 preview / execution 不共享同一 layout 时，当前计划必须阻止继续执行。

## 7. 禁止行为

以下行为在本特性中一律禁止：

1. 继续把“每段 `round(length / spacing)` + 段内落点”作为最终 truth
2. 在 geometry layout 上固定使用 `residual_mm = 0` 的逐段重算结果
3. 让闭合路径默认从几何第一个顶点开始排点
4. 把 `BuildPlannedTriggerPoints(*execution_trajectory)` 的重建结果当作 preview 主真值
5. 让 `runtime_snapshot`、`motion_trajectory_points`、显示专用采样结果或控制骨架成为成功 preview 的依据
6. 在 authority 缺失时自动把旧结果、历史结果或 legacy payload 伪装成当前布局结果

## 8. 失败条件

出现以下任一情况时，owner chain 必须失败并把失败显式传播到 preview gate：

1. 无法从输入路径中构建连续 span 或 span 总长无效
2. 闭合 span 无法在有界候选相位中得到可接受布局
3. 曲线路径无法生成受控可见路径定位结果
4. authority points 无法形成单调 execution binding
5. 同一计划下 preview 与 execution 指向不同的 authority layout
