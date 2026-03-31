# Phase 0 Research: HMI 胶点预览锚定分布一致性

## Decision 1: 权威胶点生成的 owner surface 固定在 planning / packaging owner chain

- Decision: 本特性的权威胶点生成与预览 authority 收口到 `D:\Projects\SiligenSuite\modules\dispense-packaging\application\services\dispensing\DispensePlanningFacade.cpp` 和 `D:\Projects\SiligenSuite\modules\workflow\domain\domain\dispensing\planning\domain-services\DispensingPlannerService.cpp` 所代表的 planning owner chain；`PlanningPreviewAssemblyService`、`DispensingWorkflowUseCase`、runtime-gateway 与 HMI 只消费结果，不再各自决定如何布点。
- Rationale: 当前 `PlanningPreviewAssemblyService::BuildResponse(...)` 已经只是 facade 包装，真正组装 `glue_points`、`execution_package` 和 preview payload 的逻辑在 `DispensePlanningFacade::AssemblePlanningArtifacts(...)`。如果把“首尾双锚定”和“显式 trigger authority”补在 wrapper、gateway 或 HMI，会与 execution package 分叉。
- Alternatives considered:
  - 在 HMI 渲染层做视觉重采样: 拒绝。只能改表象，无法保证执行一致性。
  - 在 `PlanningPreviewAssemblyService` wrapper 层追加布点逻辑: 拒绝。wrapper 不再拥有 planning 语义。

## Decision 2: 显式 trigger 标记后的 `interpolation_points` 成为唯一权威来源

- Decision: `interpolation_points` 上的显式 `enable_position_trigger` 标记作为 preview 与 execution 共用的唯一权威出胶结果；`trigger_distances_mm` 保留为派生兼容/导出产物，但不再作为 preview authority，也不允许在显式 trigger 缺失时回退成 preview 源。
- Rationale: 规格已澄清“预览与执行准备必须消费同一份权威出胶结果”。当前 `DispensePlanningFacade::BuildPlannedTriggerPoints(...)` 仍从 `trigger_distances_mm` 重建 preview triggers，`SelectExecutionTrajectory(...)` 仍允许 `motion_trajectory_points` fallback，这两点都与同源 authority 冲突。
- Alternatives considered:
  - 继续把 `trigger_distances_mm` 当 preview 主来源: 拒绝。会保留“预览与执行不是同一份权威结果”的结构性风险。
  - 仅在 workflow snapshot 阶段挑选最近点打标: 拒绝。不能保证几何交点与共享顶点命中。

## Decision 3: 分布算法按“几何线段首尾双锚定 + 段内均分”执行

- Decision: 对每条 `dispense_on` 几何线段独立执行首尾双锚定。设线段长度为 `L`，目标间距为 `3.0 mm`，分段数取 `n = max(1, round(L / 3.0))`，实际间距为 `L / n`，在该线段上生成包含首尾在内的 `n + 1` 个锚点；共享顶点按几何容差去重，只保留一个公共坐标。
- Scope note: 本决策是 `SPEC-001` 范围内的局部策略，不是项目级默认 span 形成规则。项目级上位策略以 `D:\Projects\SiligenSuite\docs\architecture\dxf-authority-layout-global-strategy-v1.md` 为准；当归一化结果识别为连续 span 且中间顶点属于软顶点时，应先由上位策略决定是否切分，再决定是否应用本决策。
- Rationale: 当前 trigger 计划和 preview 重建更偏向全局累计距离模型，无法从机制上保证角点命中与共享顶点共点。按线段独立锚定后，端点重合、非整倍数边长的均分稳定性，以及共享顶点去重都能被统一处理。
- Alternatives considered:
  - 从路径首点按固定步距全局累计分布: 拒绝。会在角点产生前后漂移。
  - 只允许固定 3.0 mm 严格间距: 拒绝。与非整倍数边长和短线段场景冲突。

## Decision 4: 短线段例外按“锚定后实际间距是否落出窗口”判定

- Decision: 若线段在首尾双锚定前提下、按均匀分布计算得到的实际间距落出 `2.7 mm` 到 `3.3 mm`，则该线段被标记为短线段例外；系统仍保留首尾锚点，但该段 spacing 统计与门禁说明必须显式标注为例外，而不是被当作普通违规。
- Rationale: 这是规格澄清后的明确口径，也避免人为引入另一个固定长度阈值。短线段判定与实际几何和当前目标间距直接绑定，验收时更稳定。
- Alternatives considered:
  - 使用固定长度阈值（如 `<15 mm`）判定短线段: 拒绝。与真实均分结果不总是一致。
  - 为满足常规窗口而牺牲首尾锚点: 拒绝。与几何正确性优先级冲突。

## Decision 5: spacing 复核按真实几何分组，不再把所有胶点拼成一个段

- Decision: `ValidateGlueSpacing(...)` 只负责复核，不决定生成策略；复核输入必须按真实几何线段或共享同一分布来源的 group 切分，不能再把所有 `glue_points` 包成单一 `GluePointSegment`。
- Rationale: 当前 `DispensePlanningFacade::AssemblePlanningArtifacts(...)` 把所有胶点包装成一个 `GluePointSegment` 再做 `EvaluateSpacing(...)`，这会把跨轮廓跳跃和非相邻点错误统计为间距异常，无法为短线段例外或共享顶点场景提供干净信号。
- Alternatives considered:
  - 保持单一扁平 `GluePointSegment`: 拒绝。统计会被污染，且无法支持例外规则。
  - 完全删除 spacing 复核: 拒绝。仍需要对生成结果做验收级复核。

## Decision 6: 对外 preview 契约继续冻结为 `planned_glue_snapshot + glue_points`

- Decision: gateway 和 HMI 的正式 preview 消费契约继续冻结为 `preview_source == planned_glue_snapshot` 且 `preview_kind == glue_points`；`execution_polyline` 仅为辅助叠加层，`runtime_snapshot` 继续被视为旧轨迹快照而非主预览 authority。
- Rationale: `TcpCommandDispatcher.cpp` 已经把 `planned_glue_snapshot` 与 `glue_points` 作为 `dxf.preview.snapshot` 的硬校验条件；`apps/hmi-app/src/hmi_client/ui/main_window.py` 也将 `planned_glue_snapshot` 呈现为“规划胶点主预览”，而把 `runtime_snapshot` 标为旧版轨迹快照。保持这条契约稳定，可以把本次改动聚焦在 owner 生成链。
- Alternatives considered:
  - 改回接受 `runtime_snapshot`: 拒绝。与现有协议守门和 UI 语义冲突。
  - 让 HMI 同时接受多个 preview source 作为成功来源: 拒绝。会削弱门禁的可审计性。

## Decision 7: 旧结果和旧 fallback 路径全部按失败边界处理

- Decision: 历史结果、旧格式结果、缺少来源信息的结果，以及 `motion_trajectory_points` fallback、`trigger_distances_mm` preview 重建 fallback、固定间隔采样 fallback，都定义为失败边界；不做自动转换，也不保留隐藏兼容开关。
- Rationale: 规格澄清已经明确“旧格式/历史结果一律明确失败，不做自动转换”，并把 preview failure 定义为执行门禁的一部分。继续保留自动转换或 fallback，只会把错误来源重新引入执行前路径。
- Alternatives considered:
  - 先尝试自动转换旧结果，失败再拒绝: 拒绝。会增加不可见分支和回归成本。
  - 通过环境变量保留旧 fallback 作为“兼容”路径: 拒绝。违反规则收敛方向。
