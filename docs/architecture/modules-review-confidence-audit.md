# `modules-review.md` 结论置信度审查

更新时间：`2026-03-27`

## 1. 审查边界

- 审查对象：`docs/architecture/modules-review.md`
- 审查目标：判断原报告的模块级结论、跨模块问题和总体判断是否被**现有证据**充分支撑，是否存在表述强度高于证据强度的情况。
- 证据范围：只复核原报告已使用的根级冻结文档、模块 `README.md`、`CMakeLists.txt`、目录载荷，以及少量关键实现抽样；**不**做新的全仓调用链深挖。
- 本文不判断“架构是否健康”，只判断“原报告当前写法是否稳健”。

## 2. 评级规则

| 评级 | 含义 |
|---|---|
| 高 | 结论有直接证据支撑，推断链短，当前表述强度基本合适。 |
| 中 | 结论方向大体成立，但包含明显外推、缺失性证据或抽样不足，建议降强度。 |
| 低 | 现有证据不足以支撑该结论强度，建议改成“待补证”或更弱表述。 |

## 3. 审查摘要

- 可以基本保留的高置信度结论：
  `workflow` 吸附多模块职责、`runtime-execution -> workflow` 深耦合、`dispense-packaging` 与 `workflow` 的事实双落点、`process-path` 与 `motion-planning` 的边界重叠、`topology-feature` 的层次语义错位。
- 需要降强度的结论：
  `job-ingest`、`dxf-geometry`、`process-planning`、`motion-planning`、`trace-diagnostics`、`hmi-application` 的“明显偏移/空壳化”写法偏强，改成“owner 落地不足”“控制权未完全收口”更稳妥。
- 需要明显降级为“待补证”的结论：
  `coordinate-alignment` 的“轻度偏移”。
- 原报告已经处理得比较稳妥的部分：
  `apps/planner-cli` 是否承载 owner 逻辑、HMI 是否侵入领域、`shared/` 是否业务腐化、是否存在编译期环依赖，当前都只应保留为“证据不足/未证实”。
- 总体判断“系统性漂移”方向成立，但属于综合判断，不是单条直接证据结论，建议按**中置信度**看待。

## 4. 模块级置信度矩阵

| 模块 | 原报告总体结论 | 置信度 | 主要依据 | 证据缺口 / 外推点 | 建议写法 |
|---|---|---|---|---|---|
| `workflow` | 高风险 | 高 | `modules/workflow/README.md` 明确要求 `M0` 只做编排；`modules/workflow/application/CMakeLists.txt` 直接编进 `UploadFileUseCase.cpp`、`PlanningUseCase.cpp`、`DispensingExecutionUseCase.cpp`、`DxfPbPreparationService.cpp`；实现抽样显示上传校验、DXF 存储、PB 准备、规划调用、执行计划组装都在该模块内。 | 未复核完整调用链，但不影响“边界越界”主结论。 | 保留原结论。 |
| `job-ingest` | 明显偏移 | 中 | `modules/job-ingest/CMakeLists.txt` 只有 interface target；目录载荷只有 `README.md`/`module.yaml` 和各层 README，没有实际 `.cpp/.py` payload；`workflow/UploadFileUseCase.cpp` 直接处理上传、校验、存储和 PB 准备。 | 当前只能直接证明 `M1` 本体几乎未实体化，且上传入口被 `workflow` 旁路；不能证明所有 `JobDefinition` 相关 owner 事实都缺失。 | 改成“owner 落地明显不足，上传事实被 `workflow` 旁路”。 |
| `dxf-geometry` | 明显偏移 | 中 | README 明确把 `application/engineering_data/` 设为 canonical Python owner 面；`workflow/DxfPbPreparationService.cpp` 直接绑定 `engineering-data-dxf-to-pb` 和 `scripts/engineering-data/dxf_to_pb.py`；根 `CMakeLists.txt` 仍以 interface target 暴露模块。 | 原报告把它写得过于接近“空壳”；但目录下实际已有 `application/engineering_data/*.py` 和 `adapters/dxf/*` 实质资产。能证明的是“调用控制权未完全收口”，不能直接证明“模块本体弱到近似壳”。 | 改成“`M2` owner 资产已存在，但 DXF->PB 调用控制权仍被 `workflow` 部分持有”。 |
| `topology-feature` | 轻度偏移 | 高 | `modules/topology-feature/domain/geometry/CMakeLists.txt` 的 target 和注释直接写成 `ContourAugmenterAdapter` / `adapter (infrastructure)`，但文件仍位于 `domain/geometry/`。 | 未进一步下钻更广泛的目录污染程度。 | 保留“层次语义错位/轻度偏移”。 |
| `process-planning` | 明显偏移 | 中 | README 把 `M4` 定义为 `FeatureGraph -> ProcessPlan` owner；当前事实来源只列 `domain/configuration/`；`domain/configuration/CMakeLists.txt` 仅编译配置类；`workflow/PlanningUseCase.cpp` 明确负责构造 `DispensingPlanRequest`、调用 planner、导出规划结果。 | 现有证据主要证明“owner 落地收缩成配置面”和“规划控制仍重度集中在 workflow”；是否存在未接入构建的其他规划实现，当前未核。 | 改成“`M4` owner 落地收缩为 configuration 面，规划控制仍明显集中在 `workflow`”。 |
| `coordinate-alignment` | 轻度偏移 | 低 | README owner 范围较宽，但当前事实来源只列 `domain/machine/`；`domain/machine/CMakeLists.txt` 仅编译 `DispenserModel.cpp` 与 `CalibrationProcess.cpp`。 | 这只能证明当前可见实现面偏窄，不能直接证明它已偏离 `M5` 设计边界；未核实现内容，无法判断是否已覆盖坐标变换/补偿主语义。 | 改成“现有证据仅显示 owner 面偏窄，是否构成偏移仍需补实现核查”。 |
| `process-path` | 明显偏移 | 高 | `modules/process-path/domain/trajectory/CMakeLists.txt` 直接编译 `MotionPlanner.cpp`，同时公开依赖 `SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR`；这已与 `S05` 中 `M6`/`M7` 的对象边界形成直接张力。 | 仅凭文件名不能证明它完整承担 `M7` 全部语义，但已足够证明边界重叠。 | 保留“明显偏移/边界重叠”。 |
| `motion-planning` | 明显偏移 | 中 | `modules/motion-planning/domain/motion/CMakeLists.txt` 和目录载荷都表明 `M7` 有实质运动规划实现；但 `process-path` 也有 `MotionPlanner.cpp`，`dispense-packaging/domain/dispensing/CMakeLists.txt` 也编进 `UnifiedTrajectoryPlannerService.cpp` / `DispensingPlannerService.cpp`，`workflow` 还保留 `PlanningUseCase.cpp`。 | 能高置信度证明的是“owner 唯一性被破坏”；不能直接证明 `motion-planning` 本体自身越界或“明显偏移”到何种程度。 | 改成“`M7` 自身实现充分，但规划 owner 唯一性已被破坏”。 |
| `dispense-packaging` | 明显偏移 | 中 | README 明写当前事实来源同时包括 `modules/dispense-packaging/domain/dispensing/` 和 `modules/workflow/application/usecases/dispensing/`；`domain/dispensing/CMakeLists.txt` 还编进 `UnifiedTrajectoryPlannerService.cpp`、`DispensingPlannerService.cpp`，并依赖 `process-path`、`motion-planning`、`workflow` public include。 | “事实双落点”是高置信度；“已进入 planning 责任区”主要仍由命名和依赖面推断，强度略高。 | 保留“双落点”结论；把“明显偏移”改成“耦合偏宽，且部分规划职责与 M6/M7 重叠”。 |
| `runtime-execution` | 高风险 | 高 | README 明确说 `apps/runtime-service/`、`apps/runtime-gateway/transport-gateway/` 仍在当前事实来源中；`runtime/host/CMakeLists.txt` 多处链接 `siligen_workflow_runtime_consumer_public`；`adapters/device/CMakeLists.txt` 直接链接 `siligen_workflow_domain_public` 并夹带 `legacy/drivers/multicard/*`。 | 当前直接证据主要证明“反向依赖和边界未解耦”，不是证明 `M9` 已实际重算计划对象。 | 保留“高风险”；正文宜使用“边界未彻底解耦”而不是更强的“直接越权重算”。 |
| `trace-diagnostics` | 明显偏移 | 中 | README owner 范围覆盖 trace/query/archive；但 `modules/trace-diagnostics/CMakeLists.txt` 当前只构建 `SpdlogLoggingAdapter.cpp`，目录载荷也基本只有该适配器。 | 这是典型缺失性证据：能证明 owner 面很薄，不能直接证明所有 `M10` 事实都未落地。 | 改成“owner 落地不足，目前可见实现主要停留在 logging adapter”。 |
| `hmi-application` | 明显偏移 | 中 | README 把 `M11` 定义为任务接入、审批、查询和展示 owner；`modules/hmi-application/CMakeLists.txt` 只有 interface target；目录载荷也只有各层 README，没有实质实现；`apps/hmi-app/README.md` 反而明确禁止 HMI 越权。 | 能直接证明的是“模块本体没有实体化 owner 面”；不能直接证明 HMI 宿主已接手 owner 实现。 | 改成“`M11` 模块本体尚未实体化 owner 面；宿主是否代偿承接，当前证据不足”。 |

## 5. 跨模块与总体判断矩阵

| 项目 | 原报告结论 | 置信度 | 主要依据 | 证据缺口 / 外推点 | 建议写法 |
|---|---|---|---|---|---|
| 问题 1 | `workflow` 吸附多模块 owner 事实 | 高 | README 对 `M0` 的约束与 `application/CMakeLists.txt`、`UploadFileUseCase.cpp`、`PlanningUseCase.cpp`、`DispensingExecutionUseCase.cpp`、`DxfPbPreparationService.cpp` 的现状直接矛盾。 | 无关键缺口。 | 保留原结论，`P0` 合理。 |
| 问题 2 | Runtime 对 Workflow 的反向耦合过深 | 高 | `runtime/host/CMakeLists.txt` 与 `adapters/device/CMakeLists.txt` 直接链接 `workflow` public/domain surface；`S05` 又要求 `M9` 只消费上游结果。 | 未证明存在编译期环依赖，但已足够支撑高风险耦合。 | 保留原结论，`P0` 合理。 |
| 问题 3 | 规划/轨迹 owner 重叠 | 中 | `process-path` 有 `MotionPlanner.cpp`，`motion-planning` 有完整轨迹/速度/时间规划实现，`dispense-packaging` 有 `UnifiedTrajectoryPlannerService.cpp` / `DispensingPlannerService.cpp`，`workflow` 仍有 `PlanningUseCase.cpp`。 | 部分重叠判断依赖命名和构建入口，尚未逐函数核对对象 ownership。 | 保留方向，建议把“多点实现”改成“多点持有强规划语义迹象”。 |
| 问题 4 | `dispense-packaging` 与 `workflow` 事实双落点 | 高 | `modules/dispense-packaging/README.md` 直接把两处都列为当前事实来源。 | 无关键缺口。 | 保留原结论，`P1` 合理。 |
| 问题 5 | 若干 owner 模块空壳化 | 中 | `job-ingest`、`hmi-application` 目录只有 README/锚点；`trace-diagnostics` 主要只有 spdlog adapter；`process-planning` 当前构建面主要是配置；但 `dxf-geometry` 已有真实 Python/C++ 资产。 | “若干 owner 模块空壳化”把强弱不同的模块并在一起，尤其对 `dxf-geometry` 偏强。 | 改成“若干 owner 模块落地程度明显不均，部分仅为锚点或薄实现面”。 |
| 问题 6 | `workflow` public 面正在变成隐式共享模型 | 中 | 多个模块显式包含 `${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}` 或链接 `siligen_workflow_runtime_consumer_public` / `siligen_workflow_domain_public`。 | 这是合理综合判断，但“正在变成共享模型”仍属于趋势性推断，不是直接事实。 | 改成“`workflow` public 面已表现出被多个模块共享依赖的中心化趋势”。 |
| 问题 7 | 层次语义错位 | 高 | `topology-feature/domain/geometry/CMakeLists.txt` 中 target/注释已直接表明 adapter/infrastructure 语义。 | 无关键缺口。 | 保留原结论，`P2` 合理。 |
| §5.1 | 总体判断：系统性漂移 | 中 | 高置信度局部问题至少包括 `workflow` 越界、runtime 反向耦合、`dispense-packaging` 双落点、`process-path`/`motion-planning` 重叠、`topology-feature` 层次错位。 | “系统性”依赖对 `job-ingest`、`dxf-geometry`、`process-planning`、`trace-diagnostics`、`hmi-application` 等中置信度条目的综合解释，因此不宜当成直接证据结论。 | 改成“现有证据已显示多处结构性背离，是否定性为系统性漂移可保留，但应明确这是综合判断”。 |
| §5.2-7 | `apps/planner-cli` 不是极薄壳，但 owner 证据不足 | 高 | `apps/planner-cli/CMakeLists.txt` 的链接面和 `CommandHandlers.*` 足以说明入口不薄；原报告同时明确“证据尚不足”。 | 无关键缺口，结论本身已是保守表述。 | 保留原写法。 |

## 6. 原报告中应保留的“未证实项”

以下三条，原报告当前写法是稳妥的，不建议升级为更强结论：

| 项目 | 当前结论 | 置信度 | 说明 |
|---|---|---|---|
| 编译期环依赖 | 未证实 | 高 | 当前只证实了反向耦合，没有证实真正的环。 |
| HMI 侵入领域 owner | 未证实 | 高 | `apps/hmi-app/README.md` 仍明确禁止越权；当前只证实 `modules/hmi-application` 本体很薄。 |
| `shared/` 变成业务大杂烩 | 未证实 | 高 | `shared/README.md`、`shared/CMakeLists.txt` 仍与公共契约/基础设施定位一致。 |

## 7. 建议的原报告修订口径

- 保留高强度结论：
  `workflow` 高风险、`runtime-execution` 高风险、`dispense-packaging` 与 `workflow` 双落点、`topology-feature` 层次语义错位。
- 降一级表述：
  把 `job-ingest`、`process-planning`、`trace-diagnostics`、`hmi-application` 的“明显偏移”改成“owner 落地不足/未实体化”。
- 改写为更准确的控制权结论：
  把 `dxf-geometry` 的“明显偏移”改成“owner 资产已存在，但调用控制权仍未完全从 `workflow` 收口”。
- 改写为待补证：
  把 `coordinate-alignment` 的“轻度偏移”改成“当前可见实现面偏窄，是否偏移待补证”。
- 改写为“唯一性破坏”而非“模块自身偏移”：
  把 `motion-planning` 的“明显偏移”改成“`M7` 自身实现充分，但 owner 唯一性被破坏”。
- 保留总体判断，但加限定语：
  `系统性漂移` 应明确为“基于多处中高置信度问题的综合判断”，不是单一直接证据结论。
