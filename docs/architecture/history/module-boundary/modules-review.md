# 模块架构审查

以下结论仅基于仓库内**可见文档、构建文件和少量关键实现文件**。

本审查先核对根级边界文档 `WORKSPACE.md`、`docs/architecture/workspace-baseline.md`、`docs/architecture/canonical-paths.md`、`docs/architecture/dsp-e2e-spec/*`，再回到 `modules/*` 的 `README.md`、`module.yaml`、`CMakeLists.txt` 和必要实现文件进行交叉核对。仓库当前声明的边界较为明确：`modules/` 应是业务 owner 根，`apps/` 应是进程入口/装配壳，`shared/` 应是稳定公共契约与低业务含义基础设施。

## 1. 模块总览

下面的“主要输入/输出”是基于 S05/S06、模块 README、`module.yaml` 和当前实现文件名综合判断；仓库未给出更细契约的地方，我按“推定”处理。

1. **`workflow`**：当前不只是编排层，而是同时吸纳了上传、DXF→PB 预处理、规划和执行组装。主要输入是 DXF 路径、运行参数、硬件状态；主要输出是规划结果、执行准备和运行触发。它本应只协调其他 owner，但实际边界横跨 M1/M2/M4-M9。

2. **`job-ingest`**：宣称应拥有任务接收/JobDefinition 一类事实；实际落地很弱，更像一个模块占位壳。主要输入应是上传文件与任务元数据；输出应是 intake/job 定义或其契约。边界上本应是上游入口 owner，但当前大量上传职责被 `workflow` 吸走。

3. **`dxf-geometry`**：宣称应拥有 DXF 几何与工程数据转换，README 甚至点名 `application/engineering_data/` 为 canonical Python owner 面。主要输入是 DXF；输出应是标准化几何或 PB。边界上本应是 DXF→几何/工程数据的唯一 owner，但当前 `workflow` 仍直接握有 DXF→PB 准备逻辑。

4. **`topology-feature`**：宣称应拥有拓扑/特征提取。主要输入是几何对象；输出应是 contour/feature graph 一类拓扑结构。边界上与 `dxf-geometry`、`process-planning` 相邻。当前实现里有真实代码，但目录层次出现“domain 目录下放 adapter”的偏移。

5. **`process-planning`**：宣称应把 feature/工艺规则转成 ProcessPlan。主要输入应是 feature graph 与工艺配置；输出应是 ProcessPlan。边界上本应位于 M3 与 M6/M8 之间，但当前更像配置聚合，真正规划逻辑仍大量留在 `workflow`。

6. **`coordinate-alignment`**：宣称应拥有坐标对齐、补偿、校准。主要输入应是标定/机台模型；输出应是坐标变换集与补偿参数。边界上与 planning/runtime 相邻。当前实现只覆盖了部分校准与机台模型。

7. **`process-path`**：宣称应拥有工艺路径。主要输入应是 ProcessPlan/几何；输出应是 ProcessPath 或整形后的轨迹基础。边界上本应在 motion 前。但当前模块内已出现 `MotionPlanner.cpp`，说明它不止做 path。

8. **`motion-planning`**：宣称应拥有 MotionPlan/插补规划。主要输入应是 ProcessPath；输出应是速度/时间/插补轨迹。边界上与 `process-path` 和 `dispense-packaging` 相邻。当前实现很实，但 owner 唯一性没有守住，因为 M6、M8、M0 也都在做部分规划。

9. **`dispense-packaging`**：宣称应拥有点胶执行组包/包装。主要输入应是 ProcessPath 与 MotionPlan；输出应是 Dispense/ExecutionPackage。边界上本应是 planning 与 runtime 的桥。但模块 README 已直接承认当前事实来源同时落在本模块和 `workflow/application/usecases/dispensing/`。

10. **`runtime-execution`**：宣称应拥有运行时执行。主要输入应是 execution package 与设备契约；输出应是设备控制、运行态、执行记录。边界上本应只消费稳定包，不再回头参与规划事实。实际实现对 `workflow` public surface 依赖很深，且 README 仍把 `apps/runtime-service`、`apps/runtime-gateway/transport-gateway` 算进“当前事实来源”。

11. **`trace-diagnostics`**：宣称应拥有 trace/diagnostics。主要输入应是运行与执行事件；输出应是 trace、诊断、记录。边界上本应是 runtime 的下游 owner。实际构建面目前主要只是 `spdlog` 适配器，owner 落地明显不足。

12. **`hmi-application`**：宣称应拥有 HMI 应用层。主要输入应是 application contracts；输出应是 HMI 视图行为和应用级交互。边界上应只消费 core/runtime 暴露的契约，不直接拥有 M0/M4-M9 事实。当前 `modules/hmi-application` 本身只有 interface target，owner 落地很弱。

## 2. 声明边界 vs 实际实现

### 2.1 `modules/` 是否真的是业务 owner 根

**结论：没有完全落地。**
文档层面，`WORKSPACE.md`、`workspace-baseline.md`、`canonical-paths.md`、S05/S06 都把 `modules/` 定义为业务 owner 根，且 `apps/` 不应承载一级业务 owner。实际代码里，最主要的偏差是 `workflow`：README 明说它只承载 M0 编排职责，不直接承载 M4-M8 事实；但 `modules/workflow/application/CMakeLists.txt` 直接编进了 `UploadFileUseCase.cpp`、`PlanningUseCase.cpp`、`DispensingWorkflowUseCase.cpp`、`DispensingExecutionUseCase.cpp`、`DxfPbPreparationService.cpp` 等文件，已经不是纯编排。

进一步看实现，`UploadFileUseCase.cpp` 处理 DXF 校验、文件存储、PB 准备；`DispensingExecutionUseCase.cpp` 处理硬件连接校验、运行参数刷新、PB 准备、规划调用和执行计划组装；`DxfPbPreparationService.cpp` 直接绑定 `engineering-data-dxf-to-pb` / `scripts/engineering-data/dxf_to_pb.py`。这说明 `modules/` 的 owner 根地位并非“按模块分散落地”，而是被 `workflow` 吸成了中枢 owner。

### 2.2 `apps/` 是否越权承载业务逻辑

**结论：部分落地，部分证据不足。**
`apps/runtime-service` 和 `apps/runtime-gateway` 的构建文件看起来更接近“宿主/装配壳”；但 `apps/planner-cli` 入口较厚，直接链接 `job-ingest`、`hmi-application`、`runtime_host`、`workflow_runtime_consumer_public` 等目标，并包含多类 `CommandHandlers.*`。这足以说明它不是极薄壳，但仅凭这些证据，还不足以直接定性“一级业务 owner 已经落在 app 内”。

`apps/hmi-app` 方面，README 明确禁止 HMI 直接生成或改写 M0/M4-M9 核心事实；`main.py` 更像启动/模式选择/环境初始化入口。当前证据**不足以证明** HMI 已直接侵入领域 owner；但 `modules/hmi-application` 自身又几乎是空壳，因此只能得出：**“HMI 是否越权”证据不足，但 “M11 owner 落地不足” 是确定的。**

### 2.3 `shared/` 是否混入业务语义

**结论：目前未见 `shared/` 已变成业务大杂烩的充分证据。**
根文档和 `shared/README.md` 都把 `shared/` 定义为稳定公共契约和低业务含义基础设施；当前 `shared/CMakeLists.txt` 实际只强制构建 `kernel`、`contracts`、`testing`，`shared/contracts/application|device|engineering` 的说明也基本符合“跨模块稳定协议”的定位。至少从当前证据看，`shared/` 还没有明显混入一级业务 owner。

但要补一句：`shared/ids`、`shared/artifacts`、`shared/messaging`、`shared/logging` 在当前构建层更多还是“分类根/占位根”，并未都成为独立实现面。这不是“shared 腐化”的证据，但说明共享层内部还有结构未完全收口。

### 2.4 是否有业务逻辑落在错误目录

**结论：有，且不止一处。**
最典型的是 `workflow` 下的 `UploadFileUseCase.cpp`、`PlanningUseCase.cpp`、`DispensingExecutionUseCase.cpp`、`DxfPbPreparationService.cpp`，这些职责按仓库自己的 S05/S06 和模块定义，应分别落在 `job-ingest`、`dxf-geometry`、`process-planning`/`motion-planning`/`dispense-packaging`/`runtime-execution` 等 owner 模块，而不是集中在 M0。另一个明确例子是 `modules/topology-feature/domain/geometry/CMakeLists.txt` 里 target 名与注释都写的是 `ContourAugmenterAdapter`/“adapter (infrastructure)”，却位于 `domain/geometry/` 目录。

## 3. 逐模块审查

### 3.1 `workflow`

* **宣称职责**：README 与 S05 把它定义为 M0 编排层，只做 orchestration，不直接承载 M4-M8 事实。
* **实际职责**：`application/CMakeLists.txt` 直接编译上传、规划、执行、DXF→PB 准备等 use case/service；实现文件里还包含硬件校验、PB 准备、规划调用、执行组装。
* **是否存在职责越界**：存在，而且是主问题。它已经越过了 M1/M2/M4-M9 的 owner 边界。
* **是否存在被其他模块侵入**：当前更明显的是它**主动吸纳**其他模块职责，而不是被侵入。
* **依赖方向是否合理**：不合理。它应主要依赖 contracts 和下游门面；现在却直接变成多模块事实汇聚点。
* **证据文件**：`modules/workflow/README.md`，`modules/workflow/CMakeLists.txt`，`modules/workflow/application/CMakeLists.txt`，`modules/workflow/application/usecases/dispensing/UploadFileUseCase.cpp`，`modules/workflow/application/usecases/dispensing/DispensingExecutionUseCase.cpp`，`modules/workflow/application/services/dxf/DxfPbPreparationService.cpp`。
* **结论**：**高风险**。

### 3.2 `job-ingest`

* **宣称职责**：owner 为 JobDefinition / 任务接收，app 端只是消费方。
* **实际职责**：模块根构建面目前更像 interface shell，未见自有 domain/application/adapters 作为实质 owner 面被构建。
* **是否存在职责越界**：当前证据不足以证明该模块主动越界；更明显的问题是**职责没有真正落地**。
* **是否存在被其他模块侵入**：存在。`workflow` 的 `UploadFileUseCase.cpp` 已经实际处理上传校验/存储/PB 准备。
* **依赖方向是否合理**：偏弱。一个 owner 模块只剩 interface target，且链接到更上层的 `process_runtime_core_application`，owner 根地位不稳。
* **证据文件**：`modules/job-ingest/README.md`，`modules/job-ingest/module.yaml`，`modules/job-ingest/CMakeLists.txt`，`modules/workflow/application/usecases/dispensing/UploadFileUseCase.cpp`。
* **结论**：**明显偏移**。

### 3.3 `dxf-geometry`

* **宣称职责**：DXF 几何与工程数据转换 owner，README 直接把 `application/engineering_data/` 指为 canonical Python owner 面。
* **实际职责**：根构建面仍主要是 interface/桥接；DXF→PB 准备逻辑并未完全收口在本模块。
* **是否存在职责越界**：当前更像**owner 被旁路**，而不是本模块主动越界。
* **是否存在被其他模块侵入**：存在。`workflow` 的 `DxfPbPreparationService.cpp` 直接管理 `engineering-data-dxf-to-pb` / Python 脚本调用。
* **依赖方向是否合理**：不够合理。owner 面宣称在本模块，但调用控制权仍握在 `workflow`。
* **证据文件**：`modules/dxf-geometry/README.md`，`modules/dxf-geometry/module.yaml`，`modules/dxf-geometry/CMakeLists.txt`，`modules/workflow/application/services/dxf/DxfPbPreparationService.cpp`。
* **结论**：**明显偏移**。

### 3.4 `topology-feature`

* **宣称职责**：拓扑/特征 owner，`domain/geometry/` 为唯一真实实现面。
* **实际职责**：有真实实现，但 `domain/geometry/CMakeLists.txt` 构建的 target 名与注释是 `ContourAugmenterAdapter` / “adapter (infrastructure)”。
* **是否存在职责越界**：未见明显业务越界。当前主要问题是**层次语义错位**。
* **是否存在被其他模块侵入**：证据不足。
* **依赖方向是否合理**：大体可接受，但“domain 目录下放 adapter”会让层次边界变脏。
* **证据文件**：`modules/topology-feature/README.md`，`modules/topology-feature/module.yaml`，`modules/topology-feature/CMakeLists.txt`，`modules/topology-feature/domain/geometry/CMakeLists.txt`。
* **结论**：**轻度偏移**。

### 3.5 `process-planning`

* **宣称职责**：应是 feature/规则 到 ProcessPlan 的 owner。
* **实际职责**：当前构建面实际只落了配置类：`CMPPulseConfig.cpp`、`DispensingConfig.cpp`、`InterpolationConfig.cpp`、`MachineConfig.cpp`、`ValveTimingConfig.cpp`。
* **是否存在职责越界**：本模块自身未见主动越界；更明显的是**owner 职责收缩成 config holder**。
* **是否存在被其他模块侵入**：存在。`workflow/PlanningUseCase.cpp` 承担了大量规划逻辑。
* **依赖方向是否合理**：不理想。该模块还显式包含 `${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}`，说明它在消费 `workflow` 的 public domain 面，存在 owner 倒挂。
* **证据文件**：`modules/process-planning/README.md`，`modules/process-planning/module.yaml`，`modules/process-planning/CMakeLists.txt`，`modules/process-planning/domain/configuration/CMakeLists.txt`，`modules/workflow/application/usecases/dispensing/PlanningUseCase.cpp`。
* **结论**：**明显偏移**。

### 3.6 `coordinate-alignment`

* **宣称职责**：应拥有坐标变换集、对齐补偿、校准。
* **实际职责**：当前构建文件只见 `DispenserModel.cpp` 与 `CalibrationProcess.cpp` 等实现，范围偏窄。
* **是否存在职责越界**：未见明确越界证据。
* **是否存在被其他模块侵入**：证据不足。
* **依赖方向是否合理**：一般。它也包含 `${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}`，说明依赖面仍受 `workflow` 牵制。
* **证据文件**：`modules/coordinate-alignment/README.md`，`modules/coordinate-alignment/module.yaml`，`modules/coordinate-alignment/CMakeLists.txt`，`modules/coordinate-alignment/domain/machine/CMakeLists.txt`。
* **结论**：**轻度偏移**。

### 3.7 `process-path`

* **宣称职责**：应拥有 ProcessPath / 路径整形。
* **实际职责**：模块里不仅有 `GeometryNormalizer.cpp`、`PathRegularizer.cpp`、`TrajectoryShaper.cpp`，还直接出现 `MotionPlanner.cpp`。
* **是否存在职责越界**：存在。`MotionPlanner.cpp` 说明它已经进入了 M7 的典型责任区。
* **是否存在被其他模块侵入**：存在。`motion-planning`、`dispense-packaging`、`workflow` 都在承担与路径/轨迹相关的规划职责。
* **依赖方向是否合理**：不够合理。它同样依赖 `workflow` public domain，且与 M7 边界重叠。
* **证据文件**：`modules/process-path/README.md`，`modules/process-path/module.yaml`，`modules/process-path/CMakeLists.txt`，`modules/process-path/domain/trajectory/CMakeLists.txt`。
* **结论**：**明显偏移**。

### 3.8 `motion-planning`

* **宣称职责**：应拥有 MotionPlan / 速度/时间/插补规划。
* **实际职责**：模块内有 `TrajectoryPlanner.cpp`、`SpeedPlanner.cpp`、`TimeTrajectoryPlanner.cpp`、`TriggerCalculator.cpp` 等，说明它本身确实承担了实质运动规划。
* **是否存在职责越界**：模块自身未见明显越界；问题在于**它不是唯一规划 owner**。
* **是否存在被其他模块侵入**：存在。`process-path` 有 `MotionPlanner.cpp`，`dispense-packaging` 有 `UnifiedTrajectoryPlannerService.cpp` / `DispensingPlannerService.cpp`，`workflow` 还有大体量 `PlanningUseCase.cpp`。
* **依赖方向是否合理**：部分合理。它依赖 `process-path/domain/trajectory` 可以理解，但 owner 唯一性已经被破坏。
* **证据文件**：`modules/motion-planning/README.md`，`modules/motion-planning/module.yaml`，`modules/motion-planning/CMakeLists.txt`，`modules/motion-planning/domain/motion/CMakeLists.txt`。
* **结论**：**明显偏移**。

### 3.9 `dispense-packaging`

* **宣称职责**：应拥有点胶执行组包/打包。
* **实际职责**：模块内有 `TriggerPlanner.cpp`、`DispensingController.cpp`、`UnifiedTrajectoryPlannerService.cpp`、`DispensingPlannerService.cpp`；同时 README 又承认当前事实来源还包括 `modules/workflow/application/usecases/dispensing/`。
* **是否存在职责越界**：存在一定越界。`UnifiedTrajectoryPlannerService.cpp` / `DispensingPlannerService.cpp` 已经部分进入 planning 责任区。
* **是否存在被其他模块侵入**：存在，且是公开承认的。`workflow` 仍持有一部分点胶/执行事实。
* **依赖方向是否合理**：不够合理。它同时依赖 `process-path`、`motion-planning`、`workflow` public include，耦合面较宽。
* **证据文件**：`modules/dispense-packaging/README.md`，`modules/dispense-packaging/module.yaml`，`modules/dispense-packaging/CMakeLists.txt`，`modules/dispense-packaging/domain/dispensing/CMakeLists.txt`。
* **结论**：**明显偏移**。

### 3.10 `runtime-execution`

* **宣称职责**：M9 运行时执行 owner；S05 还明确说 M9 不得重算 `ProcessPlan` / `MotionPlan` / `ExecutionPackage`。
* **实际职责**：README 仍把 `apps/runtime-service/` 与 `apps/runtime-gateway/transport-gateway/` 算进“当前事实来源”；`runtime/host/CMakeLists.txt` 多处直接 link `siligen_workflow_runtime_consumer_public`；`adapters/device/CMakeLists.txt` 直接 link `siligen_workflow_domain_public`，并夹带 `legacy/drivers/multicard/*`。
* **是否存在职责越界**：当前更准确的说法是**运行时边界没有彻底与 workflow/planning 解耦**。
* **是否存在被其他模块侵入**：存在。`workflow` public surface 已深度进入 M9。
* **依赖方向是否合理**：不合理。运行时层反向依赖 `workflow` public/domain surface，会把规划侧变成事实共享中心。
* **证据文件**：`modules/runtime-execution/README.md`，`modules/runtime-execution/module.yaml`，`modules/runtime-execution/CMakeLists.txt`，`modules/runtime-execution/runtime/host/CMakeLists.txt`，`modules/runtime-execution/adapters/device/CMakeLists.txt`。
* **结论**：**高风险**。

### 3.11 `trace-diagnostics`

* **宣称职责**：trace/diagnostics owner。
* **实际职责**：当前构建面主要只有 `adapters/logging/spdlog/SpdlogLoggingAdapter.cpp`。
* **是否存在职责越界**：未见本模块主动越界。
* **是否存在被其他模块侵入**：更准确地说是**自身 owner 落地不足**，证据不足以判断被谁侵入。
* **依赖方向是否合理**：方向本身问题不大，但 owner 面太薄，不足以支撑“trace-diagnostics 模块已落地”的结论。
* **证据文件**：`modules/trace-diagnostics/README.md`，`modules/trace-diagnostics/module.yaml`，`modules/trace-diagnostics/CMakeLists.txt`。
* **结论**：**明显偏移**。

### 3.12 `hmi-application`

* **宣称职责**：M11 HMI application owner。S05 还明确要求 M11 不是事实源，不能直接写 owner 对象，也不能绕过 M0/M9 控设备。
* **实际职责**：`modules/hmi-application/CMakeLists.txt` 只有 interface target，并链接 `siligen_application_contracts`；模块自身未见实质 owner 面。
* **是否存在职责越界**：从 `modules/hmi-application` 本体看，没有越界；问题是它**几乎没有真正落地**。
* **是否存在被其他模块侵入**：是否由 `apps/hmi-app` 承接了 M11 owner，当前证据不足；只能确认模块本体过空。
* **依赖方向是否合理**：按契约依赖本身合理，但 owner 根没有实体化。
* **证据文件**：`modules/hmi-application/README.md`，`modules/hmi-application/module.yaml`，`modules/hmi-application/CMakeLists.txt`，`apps/hmi-app/README.md`，`apps/hmi-app/src/hmi_client/main.py`。
* **结论**：**明显偏移**。

## 4. 跨模块结构问题

### 问题 1：`workflow` 吸附多模块 owner 事实

* **涉及模块**：`workflow`、`job-ingest`、`dxf-geometry`、`process-planning`、`motion-planning`、`dispense-packaging`、`runtime-execution`。
* **证据**：`workflow` README 说只做 M0 编排；但 `application/CMakeLists.txt` 直接编进上传、规划、执行、DXF→PB 准备相关实现。
* **工程后果**：owner 唯一性被打破；模块 README/`module.yaml` 与真实事实源不一致；后续重构会变成“先拆 workflow，再谈模块边界”。
* **优先级**：**P0**。

### 问题 2：Runtime 对 Workflow 的反向耦合过深

* **涉及模块**：`runtime-execution`、`workflow`。
* **证据**：`runtime/host/CMakeLists.txt` 多处链接 `siligen_workflow_runtime_consumer_public`；`adapters/device/CMakeLists.txt` 直接链接 `siligen_workflow_domain_public`。S05 又明确 M9 不应重算计划/包。
* **工程后果**：运行时边界不稳定，planning 与 runtime 很难独立演进；任何 workflow public/domain 的变动都会外溢到执行层。
* **优先级**：**P0**。

### 问题 3：规划/轨迹 owner 重叠

* **涉及模块**：`process-path`、`motion-planning`、`dispense-packaging`、`workflow`。
* **证据**：`process-path` 里有 `MotionPlanner.cpp`；`motion-planning` 里有 `TrajectoryPlanner.cpp` / `SpeedPlanner.cpp` / `TimeTrajectoryPlanner.cpp`；`dispense-packaging` 里有 `UnifiedTrajectoryPlannerService.cpp` / `DispensingPlannerService.cpp`；`workflow` 里还有大体量 `PlanningUseCase.cpp`。
* **工程后果**：同一业务概念多点实现，语义分叉、回归测试失真、优化路径不明确。
* **优先级**：**P1**。

### 问题 4：`dispense-packaging` 与 `workflow` 事实双落点

* **涉及模块**：`dispense-packaging`、`workflow`。
* **证据**：`modules/dispense-packaging/README.md` 直接写明当前事实来源同时包括 `modules/dispense-packaging/domain/dispensing/` 和 `modules/workflow/application/usecases/dispensing/`。
* **工程后果**：模块 owner 无法作为单一事实源，文档与实现长期不一致。
* **优先级**：**P1**。

### 问题 5：若干 owner 模块空壳化

* **涉及模块**：`job-ingest`、`dxf-geometry`、`process-planning`、`trace-diagnostics`、`hmi-application`。
* **证据**：这些模块的根构建面要么只有 interface target，要么只剩 config / logging adapter，和其 README/S05 中的 owner 定义不匹配。
* **工程后果**：仓库结构看起来是模块化的，但 owner 实际没有单点落地；重构时容易“按目录以为分层完成，按代码一看仍在中枢”。
* **优先级**：**P1**。

### 问题 6：`workflow` public 面正在变成隐式共享模型

* **涉及模块**：`process-planning`、`coordinate-alignment`、`process-path`、`dispense-packaging`、`runtime-execution`。
* **证据**：多个模块构建文件显式包含 `${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}` 或链接 `siligen_workflow_runtime_consumer_public` / `siligen_workflow_domain_public`。
* **工程后果**：`workflow` 从“编排层”滑向“隐式领域共享核”；一旦如此，边界修复成本会持续升高。
* **优先级**：**P1**。

### 问题 7：层次语义错位

* **涉及模块**：`topology-feature`。
* **证据**：`domain/geometry/CMakeLists.txt` 里 target/注释明确是 adapter / infrastructure。
* **工程后果**：这类问题短期不会像 P0 一样炸裂，但会持续污染“目录即边界”的可读性。
* **优先级**：**P2**。

### 需要明确说明的“未证实项”

* **环依赖**：当前证据**不足以直接证明**存在编译期环依赖；我能确认的是 `runtime-execution -> workflow public surface` 的反向耦合，以及 `workflow` 对多 owner 的吸附，这已经足以构成高风险。
* **HMI 侵入领域逻辑**：当前证据**不足以证明** HMI 已直接改写 M0/M4-M9 事实；能确认的是 `modules/hmi-application` 落地不足，而 `apps/hmi-app` 自身文档仍明确禁止这种越权。
* **`shared/` 变成大杂烩**：当前证据**不足以支持**这个结论；从 `shared/contracts`、`shared/kernel` 看，它总体仍保持在共享契约/基础设施定位。

## 5. 最终结论

### 5.1 总体判断

**总体判断：系统性漂移。**
不是单个模块轻微失真，而是仓库声明的 owner 边界与真实实现之间存在持续性背离：`workflow` 吸附多模块职责，`runtime-execution` 反向耦合 `workflow`，多个 owner 模块本体空壳化，导致 `apps/`、`modules/`、`shared/` 的三分工只部分落地。

### 5.2 最重要的 8 个问题

1. `workflow` 不是纯 M0 编排，而是多模块事实中枢。
2. `runtime-execution` 对 `workflow` public/domain surface 形成反向耦合。
3. `process-path` / `motion-planning` / `dispense-packaging` / `workflow` 同时持有规划/轨迹职责。
4. `dispense-packaging` 与 `workflow` 的事实双落点是仓库自己承认的。
5. `job-ingest`、`dxf-geometry`、`process-planning`、`trace-diagnostics`、`hmi-application` 的 owner 落地偏空。
6. `workflow` public 面正在演化为隐式共享模型。
7. `apps/planner-cli` 明显不是极薄壳，但是否已经承载 owner 逻辑，证据尚不足。
8. `topology-feature` 存在目录层次与实现语义错位。

### 5.3 结构修复顺序

**先做什么：**

1. **先收口 `workflow`**：把 `UploadFileUseCase`、`DxfPbPreparationService`、`PlanningUseCase`、`DispensingExecutionUseCase` 这类明显跨 owner 的实现逐步迁回对应模块。否则后面的边界修复都会被中枢吸回去。

2. **再切断 `runtime-execution -> workflow` 的深耦合**：让 M9 只消费稳定 contracts / package，而不是直接依赖 `workflow` public/domain surface。

3. **然后重划 M6/M7/M8 的规划边界**：明确 `process-path`、`motion-planning`、`dispense-packaging` 分别拥有哪一层对象，至少先消掉 `MotionPlanner.cpp`、`UnifiedTrajectoryPlannerService.cpp` 与 `PlanningUseCase.cpp` 之间的重复 owner。

**后做什么：**

4. **补齐空壳 owner 模块**：优先补 `process-planning`、`trace-diagnostics`、`hmi-application`，再回补 `job-ingest`、`dxf-geometry`。

5. **最后做层次清理**：例如把 `topology-feature/domain/geometry` 下的 adapter 迁到 adapter/infrastructure 目录；同时评估 `apps/planner-cli` 是否可以进一步瘦身。

### 5.4 审查证据索引表

* `WORKSPACE.md`：定义 `apps/`、`modules/`、`shared/` 三分工，是根级边界基线。
* `docs/architecture/workspace-baseline.md`：再次确认 `apps/` 仅宿主/装配，`shared/` 仅稳定公共能力。
* `docs/architecture/canonical-paths.md`：明确 `modules/` 是唯一业务 owner 根。
* `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s05-module-boundary-interface-contract.md`：给出模块 owner 原则，明确 M9/M11 禁止事项。
* `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s06-repo-structure-guide.md`：规定 `apps/` 不承载核心领域逻辑。
* `modules/workflow/README.md`：声明 `workflow` 只做 M0 编排。
* `modules/workflow/application/CMakeLists.txt`：证明 `workflow` 直接吸纳上传/规划/执行/PB 准备实现。
* `modules/workflow/application/usecases/dispensing/UploadFileUseCase.cpp`：证明上传校验/存储/PB 准备实际在 `workflow`。
* `modules/workflow/application/usecases/dispensing/DispensingExecutionUseCase.cpp`：证明硬件校验、规划调用、执行组装在 `workflow`。
* `modules/workflow/application/services/dxf/DxfPbPreparationService.cpp`：证明 DXF→PB 调用控制权仍在 `workflow`。
* `modules/process-path/domain/trajectory/CMakeLists.txt`：证明 `process-path` 含 `MotionPlanner.cpp`。
* `modules/motion-planning/domain/motion/CMakeLists.txt`：证明 M7 也在做完整轨迹/速度/时间规划。
* `modules/dispense-packaging/README.md`：直接承认本模块与 `workflow` 的事实双落点。
* `modules/runtime-execution/runtime/host/CMakeLists.txt`：证明 runtime host 反向依赖 `workflow_runtime_consumer_public`。
* `modules/runtime-execution/adapters/device/CMakeLists.txt`：证明 device adapters 反向依赖 `workflow_domain_public`，且夹带 legacy。
* `modules/process-planning/domain/configuration/CMakeLists.txt`：证明 M4 当前更像配置模块，而非完整 planning owner。
* `modules/trace-diagnostics/CMakeLists.txt`：证明 M10 当前主要只落了 logging adapter。
* `modules/hmi-application/CMakeLists.txt`：证明 M11 当前本体仅 interface target。
* `shared/CMakeLists.txt` 与 `shared/contracts/*/README.md`：支撑“`shared/` 目前未明显业务腐化”的判断。
