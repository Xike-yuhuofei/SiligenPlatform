# `modules/` Owner Boundary Review (Revised)

更新时间：`2026-03-27`

审查基线：`origin/main@df2732ac`

> 本文用于收束 `docs/architecture/modules-owner-boundary-review-2026-03-27.md` 与 `docs/architecture/modules-review-confidence-audit.md` 的最终判断。若三者表述不一致，以本文为准。

## 1. 结论摘要

- 根级边界口径是清楚的：`modules/` 应是业务 owner 根，`apps/` 只做宿主/装配，`shared/` 只做稳定公共层。
- 当前问题不在“文档不清楚”，而在“实现未按文档完全收口”。
- 本次只对 Claude 提出的 8 个问题做修正版判断，不新增第 9 个问题。
- 统一优先级重排如下：
  - `P0`：`hmi-application` 未落地、`dispense-packaging` 吸入 planning、`runtime-execution` 作用域过宽
  - `P1`：`workflow` 仍过厚、`process-path` 与 `motion-planning` 存在明确领域模型穿透
  - `P2`：`topology-feature/domain` 混入 adapter、`process-planning` owner 事实不足、`trace-diagnostics` 实现偏薄

## 2. 修正版八项判断

### 2.1 `hmi-application` 设计上是 owner，实际 owner 语义仍主要留在 `apps/hmi-app`

- 事实判断：**准确**
- 关键证据：
  - `modules/hmi-application/CMakeLists.txt` 当前只有 `INTERFACE` target，没有实体化 owner 实现面。
  - `apps/hmi-app/src/hmi_client/client/startup_sequence.py` 承载启动编排、恢复动作和会话态收敛。
  - `apps/hmi-app/src/hmi_client/features/dispense_preview_gate/preview_gate.py` 承载生产启动 gate 规则。
- 需要修正的表述：
  - 应写成“`M11` 模块本体尚未实体化 owner 面，宿主侧仍保留 owner 级启动/门禁语义”。
  - 不应直接写成“`apps/hmi-app` 已经改写 `M0/M4-M9` 核心事实”，当前证据不足以支撑这一级强度。
- 建议优先级：`P0`

### 2.2 `dispense-packaging` 内含 planning service，职责漂移明显

- 事实判断：**准确**
- 关键证据：
  - `modules/dispense-packaging/domain/dispensing/CMakeLists.txt` 直接编译 `ContourOptimizationService.cpp`、`UnifiedTrajectoryPlannerService.cpp`、`DispensingPlannerService.cpp`。
  - `modules/dispense-packaging/README.md` 明确承认当前事实来源同时包含 `modules/dispense-packaging/domain/dispensing/` 与 `modules/workflow/application/usecases/dispensing/`。
- 需要修正的表述：
  - 可以直接定性为“`M8` 已吸入部分 planning 职责，并与 `workflow` 形成事实双落点”。
  - 不需要弱化。
- 建议优先级：`P0`

### 2.3 `runtime-execution` 成为重耦合中心

- 事实判断：**基本准确，但需收敛表述**
- 关键证据：
  - `modules/runtime-execution/application/CMakeLists.txt` 私有包含 `../../workflow/application/include`。
  - `modules/runtime-execution/runtime/host/CMakeLists.txt` 在同一 owner 根内编进 config、storage、recipe persistence、diagnostics、security、container/host 组装。
  - `modules/runtime-execution/runtime/host/CMakeLists.txt` 直接公开链接 `job-ingest`、`workflow`、`trace-diagnostics` 等 public surface。
- 需要修正的表述：
  - 应写成“`M9` 当前作用域过宽且未收口，执行 owner、host glue、infrastructure glue 仍混在同一模块论证面里”。
  - 避免写成未经限定的“实现越界失控”或“已经重算 planning 对象”，因为当前主证据是边界未解耦，不是 planning 回流已被实锤。
- 建议优先级：`P0`

### 2.4 `workflow` 未完全退回纯 orchestration

- 事实判断：**准确**
- 关键证据：
  - `modules/workflow/CMakeLists.txt` 仍保留 `siligen_process_runtime_core_*` 聚合面。
  - 同文件把多个 owner 根并入 `siligen_process_runtime_core` 的拓扑属性。
  - `modules/workflow/application/` 仍持有大量 motion、dispensing、recipe、system use cases 和兼容服务。
- 需要修正的表述：
  - 应写成“`M0` 仍兼任兼容聚合总线与部分事实入口，尚未回到纯 orchestration/port 层”。
  - 这个问题不需要降强度，但应避免把所有下游实现都笼统算作 `workflow` 主动越权的同一种问题；其中一部分是迁移期兼容壳未退出。
- 建议优先级：`P1`

### 2.5 `process-path` 与 `motion-planning` 的边界不是“轻度不干净”，而是存在明确领域模型穿透

- 事实判断：**准确，而且原结论偏轻**
- 关键证据：
  - `modules/motion-planning/domain/motion/CMakeLists.txt` 明确允许直接依赖 `siligen_process_path_domain_trajectory`。
  - `modules/motion-planning/domain/motion/domain-services/MotionPlanner.h` 直接 `#include "domain/trajectory/value-objects/ProcessPath.h"`。
  - `modules/motion-planning/domain/motion/CMPCoordinatedInterpolator.h` 直接使用 `Siligen::Domain::Trajectory::ValueObjects::ProcessPath`。
- 需要修正的表述：
  - 应升级为“`M7` 直接复用 `M6 domain` 内部类型，当前是明确的领域模型穿透，不只是边界不够干净”。
  - 同时说明：问题核心不是依赖方向本身，而是 consumer 没有停在 `process-path` 的 contract/application surface。
- 建议优先级：`P1`

### 2.6 `topology-feature/domain` 混入 adapter/infrastructure 实现

- 事实判断：**准确，但优先级需调整**
- 关键证据：
  - `modules/topology-feature/domain/geometry/CMakeLists.txt` 的文件头直接写明 `Contour augmentation adapter (infrastructure)`。
  - 同文件构建 `ContourAugmenterAdapter.cpp` / `ContourAugmenterAdapter.stub.cpp`，但目录位置仍是 `domain/geometry/`。
- 需要修正的表述：
  - 应写成“这是明确的目录语义错位，不是系统主链的 owner 崩塌点”。
  - 不应再把它和 `M11/M8/M9/M0` 放在同一风险层级。
- 建议优先级：`P2`

### 2.7 `process-planning` 宣称 `ProcessPlan` owner，但当前可见 owner 事实主要收缩为 configuration 面

- 事实判断：**准确**
- 关键证据：
  - `modules/process-planning/README.md` 明确宣称 `FeatureGraph -> ProcessPlan` 与 `ProcessPlan` owner。
  - 同 README 的“当前事实来源”只列 `domain/configuration/` 与 `shared/contracts/engineering/`。
  - `modules/process-planning/application/include/process_planning/application/ProcessPlanningApplicationSurface.h` 当前只转发 `ConfigurationContracts.h`。
  - `modules/process-planning/domain/configuration/` 当前实质 payload 是配置对象与配置 port。
- 需要修正的表述：
  - 应写成“`M4` owner 事实不足，当前更多是 configuration owner，而不是完整 `ProcessPlan` owner”。
  - 不应扩大成“`process-planning` 完全无效”。
- 建议优先级：`P2`

### 2.8 `trace-diagnostics` 宣称较宽，但当前实现主要停留在 logging bootstrap + adapter

- 事实判断：**准确**
- 关键证据：
  - `modules/trace-diagnostics/README.md` 宣称 trace/query/archive/audit 范围。
  - `modules/trace-diagnostics/application/CMakeLists.txt` 当前只编译 `LoggingServiceFactory.cpp`。
  - 同文件主要公开的是 `siligen_spdlog_adapter` 与 `siligen_trace_diagnostics_application_public`。
- 需要修正的表述：
  - 应写成“`M10` 当前实现偏薄，主要是 logging bootstrap 和 adapter，不足以支撑其 README 声称的完整 owner 范围”。
  - 不需要写成“trace owner 完全不存在”；更准确的是“owner 落地明显不足”。
- 建议优先级：`P2`

## 3. 未升级为强结论的事项

- `apps/hmi-app` 是否已直接改写 `M0/M4-M9` 核心事实：**当前未证实**
- `shared/` 是否已经演化为业务实现大杂烩：**当前未证实**
- 是否已形成明确的编译期环依赖：**当前未证实**

## 4. 整改顺序

### 第一阶段：先收 `M11`

- 把启动编排、恢复策略、会话态门禁、preview/start gate 等 owner 语义从 `apps/hmi-app` 收回 `modules/hmi-application`。
- 让 `modules/hmi-application` 具备实体化的 application/contracts surface，不再只是 `INTERFACE` 空壳。
- `apps/hmi-app` 退回进程入口、Qt 生命周期、UI 装配和 launcher contract 消费。
- 退出条件：
  - `modules/hmi-application` 不再是纯空壳 target。
  - `apps/hmi-app` 不再持有 owner 级启动状态机与生产启动 gate 规则。

### 第二阶段：切开 `M8`

- 从 `modules/dispense-packaging/domain/dispensing/` 迁出 planner / optimizer / unified trajectory planning 相关实现。
- `M8` 只保留 `DispenseTimingPlan`、`TriggerPlan`、`ExecutionPackage`、离线校验和 payload 组装。
- 停止 `workflow` 继续作为 `M8` 的“当前事实来源”。
- 退出条件：
  - `dispense-packaging` 不再编译 `*Planner*` / `*Optimization*` 规划实现。
  - README 不再声明 `workflow/application/usecases/dispensing/` 是 `M8` 当前事实来源。

### 第三阶段：瘦身 `M9`

- 保留 `runtime-execution/application` 作为已规划执行输入的 canonical 执行入口。
- 将 host/container/config/storage/security/diagnostics glue 从“业务 owner 论证”中剥离，明确归到 runtime host/infrastructure 作用域。
- 消除 `runtime-execution/application` 对 `workflow/application/include` 的直接实现层感知。
- 退出条件：
  - `runtime-execution/application/CMakeLists.txt` 不再直接包含 `workflow/application/include`。
  - `M9` 只消费已规划执行输入，不再通过实现层接触 planning 细节。

### 第四阶段：收口 `M0`

- 把 `workflow` 压回 orchestration、port、compatibility shell 边界。
- 明确 `process_runtime_core_*` 仅是迁移期兼容壳，不再作为 live owner 判断依据。
- 清理 `workflow/application` 中继续承担 specialized owner 事实的 use case / service。
- 退出条件：
  - `workflow` 不再被用作多模块事实总线。
  - specialized owner 的事实判断不再依赖 `process_runtime_core_*` 聚合 target。

### 第五阶段：清理 `M6/M7` 与 `P2` 收尾项

- 为 `ProcessPath` 建立真正的 consumer surface，让 `motion-planning` 停在 `process-path` 的 contract/application 面，不再直接 include `domain/trajectory`。
- 把 `topology-feature/domain/geometry` 中的 adapter 实现移出 domain 语义目录。
- 为 `process-planning` 明确二选一：
  - 要么补齐 `ProcessPlan` owner surface
  - 要么收窄 README，只声明 configuration owner
- 为 `trace-diagnostics` 明确二选一：
  - 要么补 trace/query/archive owner surface
  - 要么收窄模块声明，只保留 logging bootstrap / adapter owner
- 退出条件：
  - `motion-planning` 不再直接依赖 `M6 domain` 内部类型。
  - `topology-feature` 目录语义恢复一致。
  - `M4`、`M10` 的 README 与实现范围重新对齐。

## 5. 最小验收口径

- 审查口径：
  - 每条判断都必须回指 `origin/main@df2732ac` 的本地证据。
  - 每条判断都明确区分“owner 落地不足”和“职责漂移/越界”。
- 实施口径：
  - `M11` 修复后，宿主层不再握有 owner 级启动/门禁规则。
  - `M8` 修复后，packaging 层不再承载 planning 服务。
  - `M9` 修复后，执行入口不再直接感知 `workflow` 实现层 include 面。
  - `M0` 修复后，兼容聚合 target 不再参与 live owner 论证。
  - `M6/M7` 修复后，`motion-planning` 只消费 `ProcessPath` contract/application surface。

