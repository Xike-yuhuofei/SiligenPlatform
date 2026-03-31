# Contract: Authority Trigger Layout

## 1. 目的

定义 planning owner chain 在本特性中必须满足的内部 authority contract：谁负责生成权威胶点布局、如何对连续路径做全局布点、闭环如何选相位、曲线如何落到真实可见路径，以及如何把同一批胶点结果绑定到 preview 与 execution。

项目级上位策略基线见：

- `D:\Projects\SiligenSuite\docs\architecture\dxf-authority-layout-global-strategy-v1.md`

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
| `closed_loop_corner_config` | 闭环显著角点策略；至少包含显著性阈值、角点聚合距离与锚点容差 |

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
4. 对闭合 span，若存在强锚点，则必须进入 `closed_loop_anchor_constrained` 模式；该模式仍是单一闭环布局，不得退回按原始几何段逐段布点。
5. `closed_loop_anchor_constrained` 必须联合求解 `interval_count` 与 `phase_mm`，而不是仅把 `phase_mm` 固定为某个几何起点或固定为 `0`。
6. 闭环内部只允许把原始拓扑顶点中的“显著角点”升级为强锚点；flatten 点、显示辅助点和平滑切向过渡点不得因闭环身份自动升格。
7. 若闭环显著角点沿弧长距离小于 `min_spacing_mm` 或显式配置的聚合距离，必须先做 `dense_corner_cluster` 聚合，再进入布局求解。
8. 布局距离必须落到最终可见路径上；line / arc 可直接解析，spline 必须先扁平化成误差受控路径再参与布局。
9. authority layout 生成后，preview 必须直接消费 `LayoutTriggerPoint.position`；execution 只能通过 `InterpolationTriggerBinding` 绑定同一批点，不能自行重建另一批点。

## 6. 校验与门禁规则

1. 间距复核必须按真实连续路径分组，不得跨不相连轮廓混算。
2. 校验结果必须区分 `pass`、`pass_with_exception`、`fail`。
3. 当闭环强锚点约束与 spacing window 冲突时，必须优先保留强锚点，再显式进入 `pass_with_exception` 或 `fail`；不得为了视觉均匀隐式丢失强锚点。
4. `pass_with_exception` 允许继续执行，但必须带明确原因。
5. `fail`、authority 缺失、binding 失败或 preview / execution 不共享同一 layout 时，当前计划必须阻止继续执行。

## 7. 禁止行为

以下行为在本特性中一律禁止：

1. 继续把“每段 `round(length / spacing)` + 段内落点”作为最终 truth
2. 在 geometry layout 上固定使用 `residual_mm = 0` 的逐段重算结果
3. 让闭合路径默认从几何第一个顶点开始排点
4. 在 `closed_loop_anchor_constrained` 下仅固定 `phase_mm=0` 就声称满足闭环锚点约束
5. 把 flatten 顶点、显示辅助点或数值噪声顶点自动升格为闭环强锚点
6. 把 `BuildPlannedTriggerPoints(*execution_trajectory)` 的重建结果当作 preview 主真值
7. 让 `runtime_snapshot`、`motion_trajectory_points`、显示专用采样结果或控制骨架成为成功 preview 的依据
8. 在 authority 缺失时自动把旧结果、历史结果或 legacy payload 伪装成当前布局结果

## 8. 失败条件

出现以下任一情况时，owner chain 必须失败并把失败显式传播到 preview gate：

1. 无法从输入路径中构建连续 span 或 span 总长无效
2. 闭合 span 在无强锚点时无法在有界候选相位中得到可接受布局
3. 闭合 span 在有强锚点时无法在 spacing window 内找到满足锚点约束的 `interval_count + phase_mm` 组合
4. 曲线路径无法生成受控可见路径定位结果
5. authority points 无法形成单调 execution binding
6. 同一计划下 preview 与 execution 指向不同的 authority layout

## 9. 闭环角点锚定策略 V1

### 9.1 目标

为 `closed_loop` 提供最小可运行的“显著角点升级为强锚点”规则，使闭环能在不退回逐段算法的前提下，对工艺显著拐角给出稳定、可追溯的命中约束。

### 9.2 显著角点判定

1. 只允许从闭环 span 的原始拓扑顶点中选取候选角点。
2. `line-line` 且非共线拐角可进入候选。
3. `line-arc`、`arc-line`、`arc-arc` 在切向不连续时可进入候选。
4. 默认显著性阈值采用偏折角 `>= 30°`。
5. 平滑切向过渡点、flatten 顶点、显示辅助点、数值噪声形成的轻微折角点不得进入候选。

### 9.3 过密角点聚合

1. 若相邻候选角点沿闭环弧长距离小于 `min_spacing_mm` 或显式配置的聚合距离，则判为 `dense_corner_cluster`。
2. 同一 cluster 内优先保留 `split_boundary` 或 `exception_boundary`。
3. 若无更高优先级边界点，则保留偏折角更大的候选角点。
4. 若仍无法区分，则按稳定拓扑顺序保留代表角点，以保证重复规划结果一致。

### 9.4 闭环求解要求

1. 无闭环角点强锚点时，使用 `closed_loop_phase_search + deterministic_search`。
2. 存在闭环角点强锚点时，使用 `closed_loop_anchor_constrained + anchor_constrained`。
3. `anchor_constrained` 的求解对象必须同时包含 `interval_count` 和 `phase_mm`。
4. 若某个可接受解能命中全部必保角点，则优先选择与 `target_spacing_mm` 偏差最小的解。
5. 若只能通过例外接受解满足锚点约束，则必须显式输出例外原因。

### 9.5 最小诊断输出

闭环 span 至少应能追溯以下诊断：

- `anchor_policy`
- `phase_strategy`
- `strong_anchor_count`
- `candidate_corner_count`
- `accepted_corner_count`
- `suppressed_corner_count`
- `dense_corner_cluster_count`
- `validation_state`
