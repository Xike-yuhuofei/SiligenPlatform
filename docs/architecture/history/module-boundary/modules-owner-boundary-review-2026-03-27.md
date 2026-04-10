# `modules/` 边界与 Owner 落点审查

更新时间：`2026-03-27`

> 修订说明：本稿为初版抽样审查。最终修正版结论与整改顺序以 [modules-owner-boundary-review-revised-2026-03-27.md](./modules-owner-boundary-review-revised-2026-03-27.md) 为准。

## 审查范围

- 只审查 `modules/` 目录，以及为理解边界所必需的少量关联文件。
- 已核对的声明性边界文件：
  - `README.md`
  - `modules/README.md`
  - `modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md`
  - `CMakeLists.txt`
  - `modules/CMakeLists.txt`
- 已抽查的关联实现文件主要来自：
  - `apps/hmi-app`
  - `apps/planner-cli`
  - `shared/contracts`

## 审查方法

- 先读声明性边界，再读实现。
- 先建立“声明边界 vs 实际实现”的对照，再做模块级判断。
- 所有结论只基于仓库内可见证据；证据不足的地方明确标注“证据不足”。

## 第一步：模块地图

| 模块名 | 推断职责 | 主要 owner / 核心对象 | 主要输入 | 主要输出 | 主要依赖对象 | 与其他模块的边界关系 | 置信度 |
|---|---|---|---|---|---|---|---|
| `workflow` | 规划链与执行链编排、桥接 legacy/runtime surfaces | `Workflow` / 编排用例 | 上游规划结果、运行期命令、DXF/配置/设备输入 | 用例执行、桥接 target、runtime consumer surface | 几乎所有 planning/execution 子域、shared | 设计上应只编排；实际已吞并多个 specialized owner | 高 |
| `job-ingest` | 任务接入、输入归档、上游接入边界 | `JobDefinition` | CLI/HMI/外部任务输入 | 标准化 job/ingest 请求 | DXF、配置、应用契约 | 设计上应在 `modules/`；实际 live 逻辑落在 `apps/` | 低 |
| `dxf-geometry` | DXF 解析、几何转换、路径源适配 | `PathSource` / DXF geometry source | DXF 文件、图形参数 | 标准化几何/路径源 | 几何库、shared types | 代码已在模块内，但 build owner 仍与 `workflow` 重叠 | 中 |
| `topology-feature` | 名义上承接拓扑特征 owner | `TopologyModel` | 几何轮廓、DXF 转换结果 | 拓扑特征或增强轮廓 | 几何转换、shared | 实际 payload 更接近 contour augment adapter | 低 |
| `process-planning` | 名义上承接工艺规划 owner | `ProcessPlan` | 工艺参数、设备参数、插补参数 | 规划配置、参数验证结果 | shared/contracts/engineering、配置端口 | 实际 payload 是配置管理，不是 `ProcessPlan` owner | 低 |
| `coordinate-alignment` | 名义上承接坐标对齐与变换 owner | `CoordinateTransformSet` | 标定输入、设备状态、机台模型 | 标定结果、设备/坐标模型 | machine/calibration 子域、shared | 实际 payload 是 machine state + calibration | 低 |
| `process-path` | 基于上游规则与对齐结果生成正式 `ProcessPath` | `ProcessPath` | 几何路径、工艺规则、对齐结果 | 路径段、路径序列、路径校验结果 | `process-planning`、`coordinate-alignment`、shared | 少数 owner 与实现落点较一致的模块 | 高 |
| `motion-planning` | 轨迹规划、插补、速度规划；声明中还混入 jog/homing/control | `MotionPlan` | `ProcessPath`、运动参数、控制接口 | 轨迹、插补结果、运动控制语义 | `process-path`、shared、motion ports | 规划与执行 payload 被拆到模块和 `workflow` 两处 | 中 |
| `dispense-packaging` | 点胶规划打包、触发计划、补偿与执行协同 | `DispensingPlan` / `TriggerPlan` | 路径、轨迹、工艺参数、阀控制接口 | 点胶计划、触发计划、执行协调语义 | `motion-planning`、`process-planning`、shared | 规划和执行能力分裂在本模块与 `workflow` 之间 | 中 |
| `runtime-execution` | 运行时宿主、设备执行、容器装配、配方/配置接入 | `ExecutionSession` 类 runtime owner | 配方、配置、运行命令、设备契约 | 运行时容器、设备交互、执行主机 | `workflow` public、device contracts、recipe/config adapters | 设计上应消费 contracts；实际反向依赖 planning/workflow | 高 |
| `trace-diagnostics` | 名义上承接 execution trace / audit / diagnostics owner | `ExecutionRecord` | 执行事件、日志、诊断数据 | 追溯记录、审计线索、诊断输出 | 日志接口、shared | 实际只看到 spdlog adapter，owner 未落地 | 中 |
| `hmi-application` | HMI 应用用例、审批/状态查询、人机语义收敛 | `UIViewModel` / HMI application state | UI 操作、审批/预览请求、runtime 状态 | 应用层请求、界面状态、preview gate 决策 | 应用契约、runtime status、workflow use cases | 设计上应在 `modules/`；实际 live gate/窗口规则在 `apps/hmi-app` | 低 |

## 第二步：声明边界 vs 实际实现

### 1. `modules/` 是否真正成为业务 owner 根

- 声明：`modules/README.md` 明确规定 `modules/` 是正式业务 owner 根，`apps/` 只保留宿主与装配，`shared/` 只保留跨模块稳定能力。
- 实际：`job-ingest` 与 `hmi-application` 为空壳；`apps/planner-cli` 与 `apps/hmi-app` 持有 live 规则与接入逻辑。
- 一致性：不一致。
- 工程后果：目录骨架表达了 owner 意图，但真实 owner 不在 owner 根，架构图失真。

### 2. `workflow` 是否仍保持“只编排不持有 owner”的边界

- 声明：`modules/workflow/README.md` 明确写明 `workflow` 不直接承载 `M4-M8` 事实实现。
- 实际：`modules/workflow/CMakeLists.txt`、`modules/workflow/domain/CMakeLists.txt`、`modules/workflow/application/CMakeLists.txt`、`modules/workflow/adapters/CMakeLists.txt` 仍直接编译 motion、dispensing、DXF、machine 等真实实现。
- 一致性：强烈不一致。
- 工程后果：`workflow` 从编排层退化为超级 owner，specialized module 无法成为单一事实源。

### 3. `job-ingest` 与 `hmi-application` 是否已从 app 壳迁回模块 owner

- 声明：两个模块 README 都把 app 定位成消费/宿主。
- 实际：两个模块的 `CMakeLists.txt` 只有 interface target；真实逻辑在 `apps/planner-cli/CommandHandlers.Dxf.cpp`、`apps/hmi-app/src/hmi_client/features/dispense_preview_gate/preview_gate.py`、`apps/hmi-app/src/hmi_client/ui/main_window.py`。
- 一致性：不一致。
- 工程后果：`apps/` 越权承载业务规则，模块骨架变成装饰性目录。

### 4. `dxf-geometry` 是否已经成为唯一 DXF owner

- 声明：`modules/dxf-geometry/module.yaml` 指向 canonical owner root。
- 实际：模块内确有 DXF adapter 实现，但模块根 target 不是唯一 owner 锚点；同名 `siligen_parsing_adapter` 同时在 `modules/dxf-geometry` 和 `modules/workflow` 内定义。
- 一致性：设计正确，落地不足。
- 工程后果：DXF 事实归属不稳定，build owner 不唯一。

### 5. 模块名与 payload 是否一致

- `topology-feature`
  - 声明：owner 是 `TopologyModel`
  - 实际：模块根只链接 contour augment adapter
  - 一致性：不一致
  - 后果：模块名无法代表 owner
- `process-planning`
  - 声明：owner 是 `ProcessPlan`
  - 实际：payload 是 configuration 管理
  - 一致性：不一致
  - 后果：规划事实没有稳定归属
- `coordinate-alignment`
  - 声明：owner 是 `CoordinateTransformSet`
  - 实际：payload 是 machine state + calibration
  - 一致性：不一致
  - 后果：alignment 与 machine domain 混装
- `trace-diagnostics`
  - 声明：owner 是 `ExecutionRecord`
  - 实际：模块根只构建 spdlog adapter
  - 一致性：不一致
  - 后果：trace owner 并未真正落地

### 6. `shared/contracts` 是否真正独立于 runtime owner

- 声明：`shared/contracts` 是跨模块稳定契约根。
- 实际：`shared/contracts/CMakeLists.txt` 把 `device` 契约指回 `modules/runtime-execution/contracts/device`。
- 一致性：不一致。
- 工程后果：稳定契约根被 runtime 私有实现反向定义。

### 7. 声明性文档是否可信

- 声明：`modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md` 应作为模块结构说明。
- 实际：文档声称 `hmi-application/src` 与 `job-ingest/src` 存在完整实现；物理目录并不存在这些文件。
- 一致性：不一致。
- 工程后果：文档会直接误导边界治理与迁移判断。

## 第三步：逐模块健康度审查

### 模块：workflow
- 宣称职责：只做 `M4-M8` 之间的编排与桥接，不直接承载 specialized owner 实现。
- 实际职责：直接编译 domain、application、adapters，并在其中承载 motion、dispensing、DXF、machine 等真实实现。
- 目录落点是否合理：不合理。
- 依赖方向是否合理：不合理。
- 是否有职责越界：有。
- 是否有被别的模块侵入：有，`runtime-execution` 直接链接其 public surfaces。
- 是否存在“空壳模块 / 名存实亡模块”风险：否，问题相反，它是超载模块。
- 证据文件：`modules/workflow/README.md`、`modules/workflow/CMakeLists.txt`、`modules/workflow/domain/CMakeLists.txt`、`modules/workflow/application/CMakeLists.txt`
- 结论：高风险

### 模块：job-ingest
- 宣称职责：任务接入与输入事实 owner。
- 实际职责：模块本身只有 interface target；CLI 侧直接承担 DXF 导入请求映射、preview 脚本编排、执行请求拼装。
- 目录落点是否合理：不合理。
- 依赖方向是否合理：不合理。
- 是否有职责越界：有，越界发生在 `apps/planner-cli`。
- 是否有被别的模块侵入：有，被 `apps/planner-cli` 实际侵入并替代。
- 是否存在“空壳模块 / 名存实亡模块”风险：有，且已发生。
- 证据文件：`modules/job-ingest/CMakeLists.txt`、`modules/job-ingest/README.md`、`apps/planner-cli/CommandHandlers.Dxf.cpp`
- 结论：高风险

### 模块：dxf-geometry
- 宣称职责：DXF/几何接入 canonical owner。
- 实际职责：模块内确有 DXF adapter 实现，但模块根 target 不是唯一 owner 锚点；`workflow` 内也定义同名 adapter target。
- 目录落点是否合理：部分合理。
- 依赖方向是否合理：不合理。
- 是否有职责越界：模块自身越界不明显；问题在 owner 不唯一。
- 是否有被别的模块侵入：有，被 `workflow` 以同名 target 和重复实现侵入。
- 是否存在“空壳模块 / 名存实亡模块”风险：否。
- 证据文件：`modules/dxf-geometry/CMakeLists.txt`、`modules/dxf-geometry/adapters/dxf/CMakeLists.txt`、`modules/workflow/adapters/infrastructure/adapters/planning/dxf/CMakeLists.txt`
- 结论：轻度偏移

### 模块：topology-feature
- 宣称职责：承接 `TopologyModel` owner。
- 实际职责：模块根只链接 contour augment adapter；核心 payload 是轮廓增强/转换，不像拓扑模型 owner。
- 目录落点是否合理：不合理。
- 依赖方向是否合理：证据不足。
- 是否有职责越界：有。
- 是否有被别的模块侵入：证据不足。
- 是否存在“空壳模块 / 名存实亡模块”风险：有。
- 证据文件：`modules/topology-feature/module.yaml`、`modules/topology-feature/CMakeLists.txt`、`modules/topology-feature/domain/geometry/CMakeLists.txt`
- 结论：明显偏移

### 模块：process-planning
- 宣称职责：承接 `ProcessPlan` owner。
- 实际职责：payload 是配置管理、参数验证、配置迁移与配置 port，不是 `ProcessPlan` 聚合。
- 目录落点是否合理：部分合理。
- 依赖方向是否合理：部分不合理。
- 是否有职责越界：有。
- 是否有被别的模块侵入：证据不足。
- 是否存在“空壳模块 / 名存实亡模块”风险：有。
- 证据文件：`modules/process-planning/domain/configuration/README.md`、`modules/process-planning/domain/configuration/CMakeLists.txt`、`modules/process-planning/domain/configuration/ports/IConfigurationPort.h`
- 结论：明显偏移

### 模块：coordinate-alignment
- 宣称职责：承接 `CoordinateTransformSet` owner。
- 实际职责：payload 是 machine model、设备状态、标定流程、硬件连接。
- 目录落点是否合理：不合理。
- 依赖方向是否合理：不合理。
- 是否有职责越界：有。
- 是否有被别的模块侵入：有，`workflow` 下存在同名 `DispenserModel.cpp` 副本。
- 是否存在“空壳模块 / 名存实亡模块”风险：有。
- 证据文件：`modules/coordinate-alignment/domain/machine/README.md`、`modules/coordinate-alignment/domain/machine/CMakeLists.txt`、`modules/workflow/domain/domain/machine/aggregates/DispenserModel.cpp`
- 结论：明显偏移

### 模块：process-path
- 宣称职责：基于上游规则和对齐结果构建正式 `ProcessPath`，作为 `M6` canonical owner。
- 实际职责：模块根直接接入 `domain/trajectory` canonical target，真实实现也在该子域。
- 目录落点是否合理：合理。
- 依赖方向是否合理：轻度偏移，仍暴露对 `workflow` public include 的兼容依赖。
- 是否有职责越界：未见明显越界。
- 是否有被别的模块侵入：轻度，`workflow/domain` 仍直接把其 canonical 子域纳入构建树。
- 是否存在“空壳模块 / 名存实亡模块”风险：否。
- 证据文件：`modules/process-path/README.md`、`modules/process-path/CMakeLists.txt`、`modules/process-path/domain/trajectory/CMakeLists.txt`
- 结论：健康

### 模块：motion-planning
- 宣称职责：README 同时覆盖轨迹规划、插补、jog、回零、运动控制等。
- 实际职责：模块 target 只编译规划/插补/calculator 子集；执行相关部分仍由 `workflow` 编译。
- 目录落点是否合理：部分合理。
- 依赖方向是否合理：不合理。
- 是否有职责越界：有。
- 是否有被别的模块侵入：有，被 `workflow` 编译并持有一部分 execution services。
- 是否存在“空壳模块 / 名存实亡模块”风险：否，但 owner 不完整。
- 证据文件：`modules/motion-planning/domain/motion/README.md`、`modules/motion-planning/domain/motion/CMakeLists.txt`、`modules/workflow/domain/domain/CMakeLists.txt`
- 结论：明显偏移

### 模块：dispense-packaging
- 宣称职责：统一承接点胶规划、触发、补偿、执行协同。
- 实际职责：模块 target 只编译规划/部分控制子集；多项执行侧文件仍由 `workflow` 编译。
- 目录落点是否合理：部分合理。
- 依赖方向是否合理：不合理。
- 是否有职责越界：有。
- 是否有被别的模块侵入：有，被 `workflow` 编译并持有一部分核心 services。
- 是否存在“空壳模块 / 名存实亡模块”风险：否，但 owner 不完整。
- 证据文件：`modules/dispense-packaging/domain/dispensing/README.md`、`modules/dispense-packaging/domain/dispensing/CMakeLists.txt`、`modules/workflow/domain/domain/CMakeLists.txt`
- 结论：明显偏移

### 模块：runtime-execution
- 宣称职责：运行时执行 host；依赖应主要是 `dispense-packaging/contracts` 与 `shared`。
- 实际职责：runtime host 直接链接 `workflow` public、直接装配 DXF adapter、recipe repositories、parameter schema、config adapters。
- 目录落点是否合理：不合理。
- 依赖方向是否合理：明显不合理。
- 是否有职责越界：有。
- 是否有被别的模块侵入：严格说是它反向侵入了 `workflow` 与 `shared/contracts` 边界。
- 是否存在“空壳模块 / 名存实亡模块”风险：否，它是超载模块。
- 证据文件：`modules/runtime-execution/module.yaml`、`modules/runtime-execution/runtime/host/CMakeLists.txt`、`modules/runtime-execution/runtime/host/bootstrap/InfrastructureBindingsBuilder.cpp`、`modules/runtime-execution/runtime/host/runtime/configuration/ConfigFileAdapter.cpp`
- 结论：高风险

### 模块：trace-diagnostics
- 宣称职责：承接 `ExecutionRecord`、追溯事件、诊断日志、审计线索归档语义。
- 实际职责：模块根只构建 `siligen_spdlog_adapter`；可见实现只是 `ILoggingService` 的 spdlog adapter。
- 目录落点是否合理：不合理。
- 依赖方向是否合理：当前可见代码未见明显反向依赖；但 owner 未落地。
- 是否有职责越界：当前不是越界，而是 owner 缺失。
- 是否有被别的模块侵入：证据不足。
- 是否存在“空壳模块 / 名存实亡模块”风险：有。
- 证据文件：`modules/trace-diagnostics/README.md`、`modules/trace-diagnostics/CMakeLists.txt`、`modules/trace-diagnostics/adapters/logging/spdlog/SpdlogLoggingAdapter.h`
- 结论：明显偏移

### 模块：hmi-application
- 宣称职责：HMI 任务接入、审批、状态查询、人机状态聚合。
- 实际职责：模块本身只有 interface target；preview gate 规则和主窗口状态协调都在 `apps/hmi-app`。
- 目录落点是否合理：不合理。
- 依赖方向是否合理：不合理。
- 是否有职责越界：有，越界发生在 `apps/hmi-app`。
- 是否有被别的模块侵入：有，被 `apps/hmi-app` 实际替代。
- 是否存在“空壳模块 / 名存实亡模块”风险：有。
- 证据文件：`modules/hmi-application/CMakeLists.txt`、`modules/hmi-application/README.md`、`apps/hmi-app/src/hmi_client/features/dispense_preview_gate/preview_gate.py`
- 结论：高风险

## 第四步：跨模块结构问题

### 问题 1：`workflow` 反向吞并 specialized owner
- 涉及模块：`workflow`、`dxf-geometry`、`process-path`、`motion-planning`、`dispense-packaging`、`coordinate-alignment`
- 证据：`modules/workflow/README.md` 与 `modules/workflow/CMakeLists.txt`、`modules/workflow/domain/CMakeLists.txt`、`modules/workflow/application/CMakeLists.txt`、`modules/workflow/adapters/CMakeLists.txt` 的现状直接冲突。
- 为什么这是结构问题而不是局部实现问题：问题发生在构建拓扑和 owner 分配层。
- 可能后果：`workflow` 成为超级模块；specialized owner 变更都必须穿过它。
- 修复优先级：P0

### 问题 2：空壳模块 + `apps/` 承载 live owner
- 涉及模块：`job-ingest`、`hmi-application`，以及 `apps/planner-cli`、`apps/hmi-app`
- 证据：两个模块的 `CMakeLists.txt` 只有 interface target；真实逻辑在 `apps/planner-cli/CommandHandlers.Dxf.cpp`、`apps/hmi-app` 的 preview gate 与主窗口逻辑中。
- 为什么这是结构问题而不是局部实现问题：入口 app 直接成为业务 owner。
- 可能后果：测试与复用只能依赖具体进程；增加新入口时重复实现继续增长。
- 修复优先级：P0

### 问题 3：规划链与执行链耦合过深且落在错误模块
- 涉及模块：`motion-planning`、`dispense-packaging`、`workflow`、`runtime-execution`
- 证据：`motion-planning` 与 `dispense-packaging` 的 README 把 planning/execution 混写；实际模块 target 只编译规划子集，而执行文件仍由 `workflow` 编译；`runtime-execution` 又直接加载 recipe/config/DXF。
- 为什么这是结构问题而不是局部实现问题：同一条业务链的规划期事实、执行期事实、运行时装配被不同错误模块持有。
- 可能后果：离线规划无法独立验证；执行侧改动会反向影响规划链。
- 修复优先级：P0

### 问题 4：同一 owner 源码双份并存，形成隐式共享模型
- 涉及模块：`workflow`、`motion-planning`、`dispense-packaging`、`dxf-geometry`、`coordinate-alignment`
- 证据：`ArcInterpolator.cpp`、`ArcTriggerPointCalculator.cpp`、`DispenserModel.cpp` 等在 `workflow` 与 specialized modules 双份并存。
- 为什么这是结构问题而不是局部实现问题：业务事实不再有唯一 owner。
- 可能后果：行为漂移、二义性回归、构建链路不一致。
- 修复优先级：P0

### 问题 5：依赖方向绕过 contracts，`runtime-execution` 反向依赖 `workflow`
- 涉及模块：`runtime-execution`、`workflow`、`process-path`
- 证据：`modules/runtime-execution/module.yaml` 只允许窄依赖；但 `runtime/host/CMakeLists.txt` 直接链接 `workflow` public；`process-path/domain/trajectory/CMakeLists.txt` 仍直接包含 `workflow` public include。
- 为什么这是结构问题而不是局部实现问题：这是模块间 contract bypass。
- 可能后果：形成高环依赖风险。
- 修复优先级：P0

### 问题 6：模块命名与 payload 语义失真
- 涉及模块：`topology-feature`、`process-planning`、`coordinate-alignment`、`trace-diagnostics`
- 证据：对应模块的 `module.yaml`、README 与 CMake payload 不一致。
- 为什么这是结构问题而不是局部实现问题：模块名是团队放置代码和判断 owner 的第一入口。
- 可能后果：新代码继续放错位置；文档和实现长期分离。
- 修复优先级：P1

### 问题 7：`shared/contracts/device` canonical 根倒挂到 `runtime-execution`
- 涉及模块：`shared/contracts`、`runtime-execution`
- 证据：`shared/contracts/CMakeLists.txt` 把 `device` 契约目录指向 `modules/runtime-execution/contracts/device`。
- 为什么这是结构问题而不是局部实现问题：稳定契约根被运行时模块反向拥有。
- 可能后果：设备契约随 runtime 私有演进而漂移。
- 修复优先级：P1

### 问题 8：声明性文档失真
- 涉及模块：`job-ingest`、`hmi-application`，以及整体模块治理
- 证据：`modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md` 声称两个模块拥有完整 `src/` 实现；实际并无这些文件。
- 为什么这是结构问题而不是局部实现问题：声明文档是架构治理的一部分。
- 可能后果：迁移任务会被误判为已完成。
- 修复优先级：P1

## 第五步：可执行结论

### A. 总体结论

`modules/` 体系目前的结论是：需要先做边界收口再继续迭代。

这不是单点偏移，而是 owner-root 设计、真实实现、构建拓扑和声明文档之间已经出现系统性不一致。

### B. 关键问题清单

1. `workflow` 继续承载 `M4-M8` 的真实实现，specialized owner 被反向吞并。
2. `job-ingest` 与 `hmi-application` 基本为空壳，`apps/planner-cli` 与 `apps/hmi-app` 持有 live owner 逻辑。
3. `runtime-execution` 绕过 contracts 直接依赖 `workflow`，并持有 recipe/config/DXF 规划期事实。
4. `motion-planning` 与 `dispense-packaging` 的 execution payload 仍编译在 `workflow`，规划链与执行链被错误拆分。
5. 多个 owner 源文件在 `workflow` 与 specialized modules 双份并存，单一事实源已失效。
6. `topology-feature`、`process-planning`、`coordinate-alignment`、`trace-diagnostics` 的模块名与实际 payload 明显失真。
7. `shared/contracts/device` 的 canonical 根倒挂到 `modules/runtime-execution`。
8. `modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md` 与实际仓库冲突，已不适合作为声明性边界依据。

### C. 修复策略

#### 第 1 阶段：先收口构建 owner 与声明边界
- 目标：让每个模块只有一个可证明的 canonical build owner；停止 `workflow`、`apps/`、`shared/` 继续制造新的 owner 事实。
- 涉及目录：`modules/workflow`、各模块根 `CMakeLists.txt`、`shared/contracts/CMakeLists.txt`、`modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md`
- 预期收益：先把“谁是 owner”说清楚并在构建上可验证。
- 风险：隐藏依赖会集中暴露。
- 是否适合一次性做完：不适合；但 `workflow` 停止继续增长必须立即执行。

#### 第 2 阶段：再迁回 live owner 实现
- 目标：把仍在 `apps/` 和 `workflow` 的 live owner 逻辑迁回对应模块。
- 涉及目录：`apps/planner-cli`、`apps/hmi-app`、`modules/job-ingest`、`modules/hmi-application`、`modules/dxf-geometry`、`modules/motion-planning`、`modules/dispense-packaging`、`modules/workflow`
- 预期收益：`apps/` 回到宿主角色；模块真正承载业务 owner。
- 风险：app 与模块之间的 API 会发生调整。
- 是否适合一次性做完：不适合；应按一条业务链一批次推进。

#### 第 3 阶段：最后统一命名、契约根与去重
- 目标：对明显语义失真的模块做 rename 或 owner 重定义；统一 `shared/contracts/device` 的 canonical 根；删除 `workflow` 与 specialized modules 的重复源码。
- 涉及目录：`modules/topology-feature`、`modules/process-planning`、`modules/coordinate-alignment`、`modules/trace-diagnostics`、`shared/contracts`、`modules/runtime-execution/contracts/device`、`modules/workflow/domain/domain`
- 预期收益：模块名、目录名、target 名与 payload 重新一致。
- 风险：涉及 target 名、include 路径、文档、测试基线同步。
- 是否适合一次性做完：不适合；必须在前两阶段稳定后再做。

### D. 审查证据索引

| 文件路径 | 得出的关键判断 | 支撑结论 |
|---|---|---|
| `README.md` | 根拓扑存在 `apps/`、`modules/`、`shared/` 三类 canonical roots | 总体声明来源 |
| `modules/README.md` | `modules/` 是正式业务 owner 根；`apps/` 只做宿主；`shared/` 只做稳定能力 | 总体结论、问题 2、问题 7 |
| `modules/workflow/README.md` | `workflow` 不应直接承载 `M4-M8` 事实实现 | 问题 1 |
| `modules/workflow/CMakeLists.txt` | `workflow` 仍编译 domain/adapters/application | 问题 1 |
| `modules/workflow/domain/CMakeLists.txt` | `workflow` 直接接入 `process-path`、`motion-planning`、`dispense-packaging` 等子域 | 问题 1 |
| `modules/workflow/application/CMakeLists.txt` | `workflow` 继续持有 motion / dispensing / system use cases | 问题 1 |
| `modules/workflow/adapters/CMakeLists.txt` | `workflow` 继续持有 DXF / geometry / redundancy adapters | 问题 1 |
| `modules/job-ingest/CMakeLists.txt` | `job-ingest` 只有 interface target | 问题 2 |
| `apps/planner-cli/CommandHandlers.Dxf.cpp` | CLI 持有 DXF 导入、preview、执行请求拼装逻辑 | 问题 2 |
| `modules/hmi-application/CMakeLists.txt` | `hmi-application` 只有 interface target | 问题 2 |
| `apps/hmi-app/src/hmi_client/features/dispense_preview_gate/preview_gate.py` | preview gate live 规则在 app | 问题 2 |
| `apps/hmi-app/src/hmi_client/ui/main_window.py` | 主窗口直接执行业务 gate 与状态协调 | 问题 2 |
| `modules/dxf-geometry/CMakeLists.txt` | 模块根未成为唯一 DXF adapter build 入口 | `dxf-geometry` 轻度偏移 |
| `modules/dxf-geometry/adapters/dxf/CMakeLists.txt` | 模块内定义 `siligen_parsing_adapter` | `dxf-geometry` owner 已部分迁入 |
| `modules/workflow/adapters/infrastructure/adapters/planning/dxf/CMakeLists.txt` | `workflow` 也定义同名 DXF adapter target | 问题 4 |
| `modules/topology-feature/CMakeLists.txt` | `topology-feature` 根只链接 contour augment adapter | 问题 6 |
| `modules/process-planning/domain/configuration/README.md` | `process-planning` 实际 payload 是配置管理 | 问题 6 |
| `modules/coordinate-alignment/domain/machine/README.md` | `coordinate-alignment` 实际 payload 是 machine/calibration | 问题 6 |
| `modules/process-path/CMakeLists.txt` | `process-path` 已以 canonical 子域为真实构建入口 | `process-path` 健康 |
| `modules/process-path/domain/trajectory/CMakeLists.txt` | `process-path` 仍残留对 `workflow` public include 的兼容依赖 | 问题 5 |
| `modules/motion-planning/domain/motion/CMakeLists.txt` | `motion-planning` 模块 target 只编译规划/插补子集 | 问题 3 |
| `modules/dispense-packaging/domain/dispensing/CMakeLists.txt` | `dispense-packaging` 模块 target 只编译规划/部分控制子集 | 问题 3 |
| `modules/runtime-execution/module.yaml` | `runtime-execution` 声明依赖应很窄 | 问题 5 |
| `modules/runtime-execution/runtime/host/CMakeLists.txt` | runtime 直接链接 `workflow` public | 问题 5 |
| `modules/runtime-execution/runtime/host/bootstrap/InfrastructureBindingsBuilder.cpp` | runtime 直接装配 DXF adapter、recipe/config repositories | 问题 3、问题 5 |
| `modules/trace-diagnostics/CMakeLists.txt` | `trace-diagnostics` 实际只有 spdlog adapter | 问题 6 |
| `shared/contracts/README.md` | `shared/contracts` 是稳定契约根 | 问题 7 |
| `shared/contracts/CMakeLists.txt` | `device` 契约被指回 `modules/runtime-execution` | 问题 7 |
| `modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md` | 文档声称 `hmi-application/src` 与 `job-ingest/src` 存在 | 问题 8 |

## 下一批应继续审查的文件路径

- `apps/trace-viewer/`
- `apps/runtime-gateway/`
- `shared/contracts/application/`
- `modules/runtime-execution/runtime/host/runtime/recipes/`

这些目录用于继续确认 M10 与 runtime 侧是否仍存在 owner 漂移，但不影响本文当前总体结论。

## 补充审查：第二轮文件核查

第二轮补查文件：

- `apps/trace-viewer/README.md`
- `apps/runtime-gateway/README.md`
- `apps/runtime-gateway/CMakeLists.txt`
- `apps/runtime-gateway/main.cpp`
- `apps/runtime-gateway/transport-gateway/README.md`
- `apps/runtime-gateway/transport-gateway/CMakeLists.txt`
- `apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpRecipeFacade.cpp`
- `apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpDispensingFacade.cpp`
- `shared/contracts/application/README.md`
- `modules/runtime-execution/runtime/host/CMakeLists.txt`
- `modules/runtime-execution/runtime/host/ContainerBootstrap.cpp`
- `modules/runtime-execution/runtime/host/bootstrap/InfrastructureBindingsBuilder.cpp`
- `modules/runtime-execution/runtime/host/container/ApplicationContainer.Recipes.cpp`
- `modules/runtime-execution/runtime/host/runtime/recipes/RecipeFileRepository.h`
- `modules/runtime-execution/runtime/host/runtime/recipes/RecipeFileRepository.cpp`
- `modules/runtime-execution/runtime/host/runtime/recipes/ParameterSchemaFileProvider.cpp`

### 补充判断 1：`apps/trace-viewer` 当前没有证据显示其承载 `M10` owner

- `apps/trace-viewer/README.md` 明确写明该目录只读展示 `modules/trace-diagnostics/` 提供的事实，不承载 `M10 trace-diagnostics` 的终态 owner 实现。
- 当前物理目录只看到 `README.md`，未看到应用代码或构建文件。
- 结论：
  - 当前没有证据显示 `apps/trace-viewer` 越权承载 `M10` owner。
  - `trace-diagnostics` 的核心问题仍然是“模块 owner 未落地”，不是“trace-viewer 已接手 owner”。

### 补充判断 2：`apps/runtime-gateway` 目前更像宿主/传输适配层，而不是新的业务 owner

- `apps/runtime-gateway/README.md` 把该目录定义为运行时网关与 TCP 入口，业务能力来源是 `transport-gateway`、`modules/runtime-execution/`、`shared/`。
- `apps/runtime-gateway/CMakeLists.txt` 只把 `siligen_transport_gateway` 和 `siligen_runtime_host` 装配成 `siligen_runtime_gateway`。
- `apps/runtime-gateway/main.cpp` 的主要行为是解析端口/配置、调用 `BuildContainer(...)`、构建 `TcpServerHost` 并启动服务。
- `apps/runtime-gateway/transport-gateway/README.md` 明确声明 transport-gateway 不负责 `M9 runtime-execution` 的 formal owner，也不负责运动、点胶、配方、系统初始化等核心业务规则。
- `TcpRecipeFacade.cpp` 和 `TcpDispensingFacade.cpp` 也显示 facades 主要是把 TCP 调用转发到 use case，不在 facade 内重新定义业务事实。
- 结论：
  - `apps/runtime-gateway` 当前更接近宿主 + 传输适配层，没有发现其直接持有新的业务 owner 事实。
  - 但 `transport-gateway` 仍通过 `siligen_workflow_runtime_consumer_public` 直接依赖 `workflow` public 面，说明 app 壳虽然没有越权成 owner，系统依赖仍未脱离 `workflow` 中心。

### 补充判断 3：`shared/contracts/application` 目前仍然是协议契约根，不是业务 owner 根

- `shared/contracts/application/README.md` 把自己定义为 HMI / CLI / TCP / Core 之间的应用层协议事实固化层，采用 JSON 契约、query/command/model/fixture/test 结构。
- 该目录当前可见内容主要是 `commands/*.json`、`queries/*.json`、`models/*.json`、`fixtures/`、`tests/`。
- 结论：
  - 当前没有证据显示 `shared/contracts/application` 已混入可执行业务 owner 实现。
  - 它仍然更像跨入口的稳定协议 source of truth。

### 补充判断 4：`runtime-execution` 下的 recipe 线不是单纯基础设施，已经形成“runtime 装配 + workflow owner”混合体

- `modules/runtime-execution/runtime/host/CMakeLists.txt` 中：
  - `siligen_runtime_recipe_persistence` 直接编译 `runtime/recipes/*` 文件。
  - 该 target 直接链接 `siligen_workflow_runtime_consumer_public`。
  - 整个 `siligen_runtime_host` 还编译 `container/ApplicationContainer.Recipes.cpp`，并继续链接 `siligen_workflow_runtime_consumer_public`。
- `RecipeFileRepository.h` 直接实现 `domain/recipes/ports/IRecipeRepositoryPort`。
- `RecipeFileRepository.cpp` 不只是文件 IO；它直接处理 `Recipe`、`RecipeVersion`、名称唯一性、归档、版本写回等规则。
- `ParameterSchemaFileProvider.cpp` 直接围绕 `ParameterSchema` 做主/备目录合并和默认 schema 解析。
- `ContainerBootstrap.cpp` 把 `IRecipeRepositoryPort`、`ITemplateRepositoryPort`、`IAuditRepositoryPort`、`IParameterSchemaPort`、`IRecipeBundleSerializerPort` 注册到运行时容器。
- `ApplicationContainer.Recipes.cpp` 在 runtime host 内直接创建 `CreateRecipeUseCase`、`UpdateRecipeUseCase`、`RecipeCommandUseCase`、`RecipeQueryUseCase`、`ImportRecipeBundlePayloadUseCase` 等 recipe use cases。
- 进一步全仓定位显示：
  - `IRecipeRepositoryPort.h` 物理文件位于 `modules/workflow/domain/domain/recipes/ports/` 与 `modules/workflow/domain/include/domain/recipes/ports/`
  - `RecipeQueryUseCase.h` 位于 `modules/workflow/application/usecases/recipes/` 与 `modules/workflow/application/include/application/usecases/recipes/`
  - `RecipeJsonSerializer.h` 位于 `modules/workflow/adapters/recipes/serialization/` 与 `modules/workflow/adapters/include/recipes/serialization/`
- 结论：
  - recipe 这条线当前没有独立模块 owner。
  - 实际结构是：`shared/contracts/application` 暴露 recipe 协议，`workflow` 持有 recipe domain/use case/serializer，`runtime-execution` 持有 recipe repository/schema/audit 的运行时装配与基础设施。
  - 这不是健康分层，而是一个跨 `shared -> workflow -> runtime-execution -> apps/runtime-gateway` 的隐式共享子系统。

### 第二轮补充后的结论修正

- `trace-diagnostics`
  - 仍应判断为 owner 未落地。
  - 当前没有证据表明 `apps/trace-viewer` 越权承接了 `M10` owner。
- `apps/runtime-gateway`
  - 目前不构成新的 owner 越权主问题。
  - 但它强化了 `runtime-execution -> workflow` 的 public 依赖链。
- 新增结构问题：
  - “recipe 子系统无独立 owner，跨 `workflow` 与 `runtime-execution` 隐式共享”
  - 优先级：`P0`
  - 原因：它已经同时涉及 domain/use cases、runtime repositories、transport facade 和 application contracts，属于跨层共享模型，不是局部代码位置问题。
