# Phase 0 Research: 胶点全局均布与真值链一致性

## Decision 1: 权威胶点布局 owner surface 下沉到 `modules/dispense-packaging` 的 domain planning 层

- Decision: 新增 `AuthorityTriggerLayoutPlanner` 作为几何布局真值 owner，放在 `D:\Projects\SiligenSuite\modules\dispense-packaging\domain\dispensing\planning\domain-services\`；`DispensePlanningFacade.cpp` 与 `DispensingPlannerService.cpp` 只负责调用该布局服务并把结果绑定到执行与预览链。
- Rationale: 当前 `DispensePlanningFacade.cpp` 和 `DispensingPlannerService.cpp` 内都有 `BuildTriggerArtifacts(...)` 与基于 `AppendAnchoredAuthorityForSegment(...)` 的布局逻辑，application 层已经承载了过多几何决策。把“连续路径全局均布、闭环相位、曲线定位”留在 facade 或 workflow 层，只会继续制造重复实现和真值漂移。
- Alternatives considered:
  - 继续在 `DispensePlanningFacade.cpp` 内扩展 `AppendAnchoredAuthorityForSegment(...)`: 拒绝。仍然把 domain 规则塞在 application orchestration 内，且与 `DispensingPlannerService.cpp` 重复。
  - 在 `modules/workflow` 或 HMI 末端重建几何布局: 拒绝。consumer 不应成为布局 owner，无法保证执行一致性。

## Decision 2: 布点单位从单个 segment 升级为连续 dispense span

- Decision: 布局输入改为按连续 `dispense_on` span 聚合后的路径单元，而不是对每个内部 segment 独立 `round(length / spacing)`。每个 span 一次性求总长、目标间距、实际间距、候选点数和全局距离序列。
- Rationale: 当前 `DispensePlanningFacade.cpp` 在 `for (segment ...)` 循环中对每段调用 `TriggerPlanner.Plan(..., residual_mm = 0.0f, ...)`，随后又调用 `AppendAnchoredAuthorityForSegment(...)`。这说明现有布局本质上仍是“按段决定点数，再段内落点”，分段方式会直接污染结果。
- Alternatives considered:
  - 只把 `TriggerPlanner.residual_mm` 串起来跨段传递: 拒绝。它能改善开放路径连续性，但不能单独解决闭环无固定起点、强锚点和曲线真实定位问题。
  - 保持当前按段独立布点，只调容差参数: 拒绝。不能从结构上消除分段敏感性。

## Decision 3: `TriggerPlanner` 继续负责时序/安全，几何等距由新的 authority layout 决定

- Decision: `TriggerPlanner` 保留为时间/脉冲安全与触发节拍规划器；几何等距序列由 `AuthorityTriggerLayoutPlanner` 先求得，之后再把同一序列绑定到插补点列或执行命令。
- Rationale: `TriggerPlanner.h` 已暴露 `TriggerSpacingPlan.distances_mm` 与 `residual_mm`，但它的输入输出仍是单段长度和触发安全参数。把几何布局、闭环相位和异常分类都继续塞进 `TriggerPlanner`，会扩大它与时序职责的耦合。
- Alternatives considered:
  - 把闭环相位、曲线定位与强锚点规则直接加入 `TriggerPlanner`: 拒绝。时序/安全服务不适合拥有完整几何布局职责。
  - 完全绕开 `TriggerPlanner`: 拒绝。执行安全边界和脉冲间隔仍需要保留现有 domain 能力。

## Decision 4: 闭合路径采用有界候选相位搜索，并使用确定性 tie-break

- Decision: 对闭合 span 采用固定数量的候选相位采样，例如每个实际间距拆成 16 或 32 个候选偏移；评分优先最大化胶点到非强锚点顶点/缝口的最小距离，并使用最小相位作为稳定 tie-break。
- Rationale: 规格要求闭环在没有业务强制起点时不得依赖固定起点。若固定从 phase 0 开始，任何同形闭环都会在缝口或尖角附近重复出现局部堆点。候选相位搜索是实现成本和结果稳定性之间最合适的折中。
- Alternatives considered:
  - 固定闭环从第一个几何点开始布点: 拒绝。会把内部几何起点暴露为用户可见偏置。
  - 引入连续优化器求最优相位: 暂不采用。对当前 feature 来说收益不足以覆盖复杂度。

## Decision 5: 曲线场景采用“最终可见路径”定位，spline 先误差受控扁平化

- Decision: line / arc 继续按真实几何长度与位置解析；spline 统一先经过误差受控扁平化，再进入 authority layout 的弧长定位。扁平化误差使用现有 `spline_max_error_mm` 语义或保守默认值。
- Rationale: 当前代码路径显示，`PreviewSnapshotService` 仍基于运行时 polyline 采样，而旧的布局链对曲线存在控制骨架污染风险。对 spline 直接做精确弧长参数化成本高、改动面大；先扁平化成受控可见路径，能更快满足“胶点沿最终可见路径分布”的需求。
- Alternatives considered:
  - 继续按控制骨架或中间 seed points 决定胶点位置: 拒绝。与规格对“真实可见路径”的要求冲突。
  - 一步到位实现 spline 精确弧长参数化: 暂不采用。实现复杂度高于当前 feature 需要。

## Decision 6: 预览、校验与执行绑定共享同一批 authority points，预览不再从轨迹二次重建

- Decision: `glue_points` 直接来自 `AuthorityTriggerLayout.points`；执行层通过 `InterpolationTriggerBinding` 将同一批 points 单调映射到 `interpolation_points`；spacing 校验直接针对 authority points 及其 span/group 结果，而不再从 `BuildPlannedTriggerPoints(*trajectory)` 重建预览点集。
- Rationale: 当前 `DispensePlanningFacade.cpp` 先生成 `trigger_artifacts`，再把 markers 回贴到插补轨迹，最后又从 `selection.execution_trajectory` 调 `BuildPlannedTriggerPoints(...)` 和 `CollectTriggerPositions(...)` 重建 `glue_points`。这使“预览看到的点集”和“校验/执行使用的点集”并不天然一致。
- Alternatives considered:
  - 保持 `BuildPlannedTriggerPoints(...)` 作为 preview truth: 拒绝。显示结果会继续依赖插补离散度和 marker 匹配误差。
  - 让 HMI 自行根据 execution polyline 重采样: 拒绝。consumer 不得再定义 authority。

## Decision 7: 间距校验采用三态结果，允许显式例外但阻止未解释违规

- Decision: spacing 校验结果在内部按 `pass`、`pass_with_exception`、`fail` 三态建模。允许保留“几何正确但不满足常规窗口”的例外，只要例外被分类并附带原因；门禁只阻止 `fail` 或 authority 缺失/失配，不把所有例外都当成阻塞错误。
- Rationale: 规格明确允许显式、可解释的间距例外。若继续把校验结果简化为单一布尔值，后续实现容易把“允许例外”与“真正违规”混为一谈。
- Alternatives considered:
  - 只保留单一 `preview_spacing_valid` 布尔语义: 拒绝。不足以表达允许例外的业务规则。
  - 完全去掉间距校验，只保留几何布局: 拒绝。执行前仍需要可复核的验收结论。

## Decision 8: 对外 `dxf.preview.snapshot` schema 保持稳定，但 transport 兼容 shim 不再扩展为 truth source

- Decision: 对外响应继续冻结为 `preview_source=planned_glue_snapshot`、`preview_kind=glue_points`、非空 `glue_points`；`execution_polyline` 仅作辅助叠加。现有 `TcpCommandDispatcher.cpp` 中“缺少 `plan_id` 时先 prepare 一次”的兼容分支视为现有 transport shim，本 feature 不扩展它，也不允许它绕开 authority 规则。
- Rationale: gateway、HMI 与 formal schema 已经围绕 `planned_glue_snapshot + glue_points` 建立了稳定成功形态。为了专注解决布局真值问题，不应在本 feature 同时推翻对外 schema；但现有 transport shim 也不能重新变成新的 truth fallback。
- Alternatives considered:
  - 改变 `dxf.preview.snapshot` 的成功字段或主预览类型: 拒绝。会扩大变更面并破坏现有 contract 测试。
  - 继续允许 `runtime_snapshot`、空 `glue_points` 或显示专用采样结果进入成功路径: 拒绝。与当前 formal schema、gateway 校验和 HMI 门禁相冲突。
