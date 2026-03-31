# process-planning 模块架构审查

- 审查模块：`modules/process-planning/`
- 审查时间：`2026-03-31 07:52:01 +08:00`
- 审查范围：
  - 必查：`modules/process-planning/`
  - 少量关联：`README.md`、`modules/README.md`、`modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md`、`CMakeLists.txt`
  - 强关联少量代码：`modules/workflow/`、`modules/dispense-packaging/`、`modules/job-ingest/`、`modules/dxf-geometry/`、`modules/runtime-execution/`、`apps/runtime-service/`、`apps/runtime-gateway/`、`apps/planner-cli/`
- 审查原则：
  - 先读声明性边界，再读实现
  - 只基于可见证据下结论
  - 证据不足处明确标注

## 1. 模块定位

- 从模块本地文档和代码看，`modules/process-planning/` 当前实际核心职责不是“工艺规划/工艺决策”，而是“配置类型定义 + 配置端口 + 兼容桥接”。
  - 证据：`modules/process-planning/README.md:7-9`
  - 证据：`modules/process-planning/module.yaml:3`
  - 证据：`modules/process-planning/domain/configuration/ports/IConfigurationPort.h:205-401`
- 从仓库冻结文档看，它在业务链中本应位于 `FeatureGraph -> ProcessPlan -> CoordinateTransformSet` 的 `ProcessPlan` 阶段。
  - 证据：`docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s05-module-boundary-interface-contract.md:227-254`
  - 证据：`docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s06-repo-structure-guide.md:505-518`
  - 证据：`modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md:747-750`
- 实际主要输入不是 `FeatureGraph`、recipe、process template，而是配置文件与配置项。
  - 证据：`modules/process-planning/domain/configuration/ports/IConfigurationPort.h:51-199`
  - 证据：`apps/runtime-service/runtime/configuration/ConfigFileAdapter.cpp:48-87`
- 实际主要输出不是 `ProcessPlan`，而是 `DispensingConfig`、`MachineConfig`、`HomingConfig`、`DxfPreprocessConfig`、`IConfigurationPort`。
  - 证据：`modules/process-planning/domain/configuration/ports/IConfigurationPort.h:51-401`
- 按冻结口径，它应依赖 `topology-feature/contracts`、`shared`。
  - 证据：`modules/process-planning/module.yaml:6-10`
- 按现状，它还隐式依赖 workflow 的 motion/safety 类型。
  - 证据：`modules/process-planning/domain/configuration/value-objects/ConfigTypes.h:3-5`
  - 证据：`CMakeLists.txt:386-396`
- 按冻结口径，它应主要被 M5-M8 消费。
  - 证据：`docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s06-repo-structure-guide.md:545-556`
- 按现状，M1、M2、M8、M9 和多个 app 都在消费它。
  - 证据：`modules/job-ingest/CMakeLists.txt:39-42`
  - 证据：`modules/dxf-geometry/CMakeLists.txt:25-27`
  - 证据：`modules/runtime-execution/application/CMakeLists.txt:32-37`
  - 证据：`apps/runtime-gateway/CMakeLists.txt:21-26`
  - 证据：`apps/planner-cli/CMakeLists.txt:21-30`

## 2. 声明边界 vs 实际实现

- 仓库全局文档对 M4 的定义：
  - M4 是 `ProcessPlan` owner。
  - 负责把 `FeatureGraph` 映射为工艺规则并生成 `ProcessPlan`。
  - 对外输入应包括 `PlanProcess(...)`、`ReplanProcess(...)`。
  - 对外输出应包括 `ProcessPlanBuilt`、`ProcessRuleRejected`。
  - 证据：`docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s05-module-boundary-interface-contract.md:227-246`
  - 证据：`modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md:747-750`

- 模块本地文档对自己的定义：
  - 它只是“工艺配置、规划参数与运行前配置事实”的 owner。
  - 明确“不越权声明完整 `ProcessPlan` owner”。
  - 明确“不在本轮承担 `FeatureGraph -> ProcessPlan` 全链路规划实现口径”。
  - 证据：`modules/process-planning/README.md:7-9`
  - 证据：`modules/process-planning/application/README.md:3-7`
  - 证据：`modules/process-planning/contracts/README.md:7-14`

- 实际代码真正做的事：
  - 根 target 只加载 `domain/configuration`、`contracts`、`application`。
  - `application` 只是转发 `ConfigurationContracts.h`。
  - `contracts` 只是转发 legacy bridge 的 `IConfigurationPort`。
  - 5 个 `.cpp` 文件全部是占位符。
  - 证据：`modules/process-planning/CMakeLists.txt:1-47`
  - 证据：`modules/process-planning/application/include/process_planning/application/ProcessPlanningApplicationSurface.h:1-3`
  - 证据：`modules/process-planning/contracts/include/process_planning/contracts/ConfigurationContracts.h:1-3`
  - 证据：`modules/process-planning/domain/configuration/CMPPulseConfig.cpp:1`
  - 证据：`modules/process-planning/domain/configuration/DispensingConfig.cpp:1`
  - 证据：`modules/process-planning/domain/configuration/InterpolationConfig.cpp:1`
  - 证据：`modules/process-planning/domain/configuration/MachineConfig.cpp:1`
  - 证据：`modules/process-planning/domain/configuration/ValveTimingConfig.cpp:1`

- 两者是否一致：
  - 与模块本地文档一致。
  - 与仓库全局文档不一致。

- 偏差在哪里：
  - 模块名和冻结文档宣称它是“工艺规划模块”。
  - 模块本体却没有 `FeatureGraph -> ProcessPlan` 的任何公开接口、事件、模型、用例或实现。
  - 证据：`modules/process-planning/README.md:8-9`
  - 证据：`modules/process-planning/application/README.md:7`
  - 证据：`modules/process-planning/contracts/README.md:9-13`
  - 证据：`modules/process-planning/` 内对 `ProcessPlan|FeatureGraph|PlanProcess|ReplanProcess|ProcessPlanBuilt|ProcessRuleRejected` 无实现匹配

- 是否存在该模块应有实现却落在别处：
  - 对于“configuration owner”这一事实，成立。真正的读取、持久化、校验落在 `apps/runtime-service/`。
  - 证据：`apps/runtime-service/CMakeLists.txt:43-76`
  - 证据：`apps/runtime-service/runtime/configuration/ConfigFileAdapter.h:30-99`
  - 证据：`apps/runtime-service/runtime/configuration/ConfigFileAdapter.cpp:48-127`
  - 证据：`apps/runtime-service/runtime/configuration/ConfigFileAdapter.Sections.cpp:15-330`
  - 证据：`apps/runtime-service/runtime/configuration/ConfigFileAdapter.Sections.cpp:476-560`
  - 对于“完整 `ProcessPlan` owner”，在当前限制范围内证据不足，未发现明确替代 owner 实现。

- 是否存在别的模块的 owner 事实被放进该模块：
  - 成立。
  - `IFileStoragePort` 属于文件接入/存储抽象，更接近 M1。
    - 证据：`modules/process-planning/domain/configuration/ports/IFileStoragePort.h:22-91`
    - 证据：`modules/job-ingest/application/include/application/usecases/dispensing/UploadFileUseCase.h:28-47`
  - `DxfPreprocessConfig` 与 DXF -> PB 的预处理参数，更接近 M2。
    - 证据：`modules/process-planning/domain/configuration/ports/IConfigurationPort.h:93-117`
    - 证据：`modules/dxf-geometry/application/services/dxf/DxfPbPreparationService.cpp:551-584`
  - `HardwareMode`、`HardwareConfiguration`、`DiagnosticsConfig`、回零参数，更接近 M9/runtime 配置面。
    - 证据：`modules/process-planning/domain/configuration/ports/IConfigurationPort.h:264-401`

- 是否存在该模块只是“目录存在”，但真实实现散落在外部：
  - 成立。
  - 模块内 `.cpp` 是占位符。
  - app 层承担真实配置实现。
  - workflow 保留重复 bridge 头与重复配置模型。
  - 证据：`modules/process-planning/domain/configuration/CMPPulseConfig.cpp:1`
  - 证据：`modules/process-planning/domain/configuration/DispensingConfig.cpp:1`
  - 证据：`modules/process-planning/domain/configuration/InterpolationConfig.cpp:1`
  - 证据：`modules/process-planning/domain/configuration/MachineConfig.cpp:1`
  - 证据：`modules/process-planning/domain/configuration/ValveTimingConfig.cpp:1`
  - 证据：`apps/runtime-service/runtime/configuration/ConfigFileAdapter.h:30-99`
  - 证据：`apps/runtime-service/runtime/configuration/ConfigFileAdapter.cpp:48-127`
  - 证据：`apps/runtime-service/runtime/configuration/ConfigFileAdapter.Sections.cpp:15-80`
  - 证据：`apps/runtime-service/runtime/configuration/ConfigFileAdapter.Sections.cpp:476-560`
  - 证据：`modules/workflow/domain/include/domain/configuration/ports/IConfigurationPort.h:1-3`
  - 证据：`modules/workflow/domain/domain/configuration/value-objects/ConfigTypes.h:1-430`

- 是否存在与其它模块重复建模、重复流程、重复数据定义：
  - 成立。
  - `modules/workflow/domain/domain/configuration/value-objects/ConfigTypes.h` 保留了一套几乎相同的配置模型。
  - `git diff --no-index` 仅显示 include 路径差异；具体 diff 摘录见附录。
  - 证据：`modules/process-planning/domain/configuration/value-objects/ConfigTypes.h:1-430`
  - 证据：`modules/workflow/domain/domain/configuration/value-objects/ConfigTypes.h:1-430`
  - 证据：`modules/workflow/domain/domain/configuration/README.md:1-54`

## 3. 模块内部结构审查

- 明确判断：混乱

- 原因一：目录骨架齐全，但 `application`、`contracts` 只是转发表面，不是业务 owner 面。
  - 证据：`modules/process-planning/CMakeLists.txt:26-47`
  - 证据：`modules/process-planning/application/include/process_planning/application/ProcessPlanningApplicationSurface.h:1-3`
  - 证据：`modules/process-planning/contracts/include/process_planning/contracts/ConfigurationContracts.h:1-3`

- 原因二：公共接口与内部实现没有清晰分离，接口面直接暴露大量跨域配置细节。
  - 证据：`modules/process-planning/domain/configuration/ports/IConfigurationPort.h:205-401`

- 原因三：领域逻辑、应用逻辑、适配器逻辑并未在模块内收敛。
  - 模块内只剩端口和 DTO。
  - 真正的文件读取、配置解析、校验在 `apps/runtime-service/`。
  - 证据：`modules/process-planning/domain/README.md:1-3`
  - 证据：`apps/runtime-service/runtime/configuration/ConfigFileAdapter.cpp:48-127`
  - 证据：`apps/runtime-service/runtime/configuration/ConfigFileAdapter.Sections.cpp:15-330`

- 原因四：存在“工具/配置堆积”掩盖真实职责。
  - `IConfigurationPort` 同时覆盖点胶、DXF、诊断、机器、回零、阀门、速度采样、备份恢复、硬件模式。
  - 证据：`modules/process-planning/domain/configuration/ports/IConfigurationPort.h:245-401`

- 原因五：模块自述与实现依赖矛盾。
  - README 声称 `domain/configuration` 不依赖其它子域。
  - `ConfigTypes.h` 却直接包含 motion/safety 子域类型。
  - 证据：`modules/process-planning/domain/configuration/README.md:51-54`
  - 证据：`modules/process-planning/domain/configuration/value-objects/ConfigTypes.h:3-5`

- 原因六：CMake 没有显式表达真实依赖，依赖靠根级全局 include 注入。
  - 证据：`modules/process-planning/domain/configuration/CMakeLists.txt:11-19`
  - 证据：`CMakeLists.txt:386-396`

## 4. 对外依赖与被依赖关系

### 合理依赖

- `dispense-packaging` 读取点胶/机器配置作为下游运行参数，方向上可接受。
  - 证据：`modules/dispense-packaging/domain/dispensing/domain-services/SupplyStabilizationPolicy.cpp:12-33`
  - 证据：`modules/dispense-packaging/domain/dispensing/domain-services/DispensingProcessService.cpp:238-320`
  - 工程后果：作为“下游只读消费配置”是合理的。

### 可疑依赖

- `apps/planner-cli`、`apps/runtime-gateway` 直接链接 `siligen_process_planning_legacy_configuration_bridge_public`，消费的是 legacy bridge，不是 canonical application surface。
  - 证据：`apps/planner-cli/CMakeLists.txt:21-30`
  - 证据：`apps/runtime-gateway/CMakeLists.txt:21-26`
  - 工程后果：bridge 退出会被 app 直接阻塞。

### 高风险依赖

- `job-ingest` 直接链接 `siligen_process_planning_domain_configuration` 与 legacy bridge。
  - 证据：`modules/job-ingest/CMakeLists.txt:39-42`
  - 工程后果：M1 对 M4 concrete 反向耦合，破坏 `job-ingest -> dxf-geometry -> topology-feature -> process-planning` 的链路方向。

- `job-ingest` 上传用例直接使用 `IFileStoragePort` 和 `IConfigurationPort`。
  - 证据：`modules/job-ingest/application/include/application/usecases/dispensing/UploadFileUseCase.h:28-47`
  - 证据：`modules/job-ingest/application/usecases/dispensing/UploadFileUseCase.cpp:29-40`
  - 工程后果：M4 侵入 M1 的文件接入职责。

- `dxf-geometry` 直接链接 `siligen_process_planning_domain_configuration`。
  - 证据：`modules/dxf-geometry/CMakeLists.txt:25-27`
  - 工程后果：M2 对 M4 concrete 反向耦合。

- `dxf-geometry` 的 `DxfPbPreparationService` 通过 `GetDxfPreprocessConfig()` 读取 M4 配置。
  - 证据：`modules/dxf-geometry/application/services/dxf/DxfPbPreparationService.cpp:551-584`
  - 工程后果：DXF 预处理策略和工艺模块绑定，模块边界漂移。

- `process-planning` 自身隐式依赖 workflow 的 motion/safety 类型，而 workflow 又反向依赖 process-planning contracts。
  - 证据：`modules/process-planning/domain/configuration/value-objects/ConfigTypes.h:3-5`
  - 证据：`modules/workflow/domain/CMakeLists.txt:3-8`
  - 证据：`modules/workflow/domain/include/domain/configuration/ports/IConfigurationPort.h:1-3`
  - 工程后果：存在编译期环依赖风险，且依赖图无法从 target 声明中直观看出。

- M4 的真实配置实现放在 `apps/runtime-service`。
  - 证据：`apps/runtime-service/CMakeLists.txt:43-76`
  - 证据：`apps/runtime-service/runtime/configuration/ConfigFileAdapter.h:30-99`
  - 工程后果：违反“apps 只保留宿主和装配职责”，owner root 与实现 root 脱节。

- `runtime-execution` application 也直接链接 M4 legacy configuration bridge。
  - 证据：`modules/runtime-execution/application/CMakeLists.txt:32-37`
  - 证据：`modules/runtime-execution/application/CMakeLists.txt:72-82`
  - 工程后果：运行时应用层与 M4 配置 bridge 紧耦合，M4 语义继续向下游扩散。

## 5. 结构性问题清单

- 问题名称：M4 角色漂移且 owner 空缺
  - 现象：
    - 全局文档定义 M4 为 `ProcessPlan` owner。
    - 模块本地文档与代码却把它收缩为 configuration owner。
    - 模块内无 `ProcessPlan` 模型、契约、用例、事件和实现。
  - 涉及文件：
    - `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s05-module-boundary-interface-contract.md:227-246`
    - `modules/process-planning/README.md:7-9`
    - `modules/process-planning/module.yaml:3-10`
    - `modules/process-planning/application/README.md:3-7`
    - `modules/process-planning/CMakeLists.txt:1-47`
  - 为什么这是结构问题而不是局部实现问题：
    - 这是模块身份定义与依赖图的失真，不是单个类未实现。
  - 可能后果：
    - 业务链 S5 无稳定 owner。
    - 后续阶段长期绕过 M4。
  - 优先级：P0

- 问题名称：owner 实现外置到 app 层
  - 现象：
    - 模块内 `.cpp` 全是占位。
    - 配置解析、持久化、校验落在 `apps/runtime-service/`。
  - 涉及文件：
    - `modules/process-planning/domain/configuration/CMPPulseConfig.cpp:1`
    - `modules/process-planning/domain/configuration/DispensingConfig.cpp:1`
    - `modules/process-planning/domain/configuration/InterpolationConfig.cpp:1`
    - `modules/process-planning/domain/configuration/MachineConfig.cpp:1`
    - `modules/process-planning/domain/configuration/ValveTimingConfig.cpp:1`
    - `apps/runtime-service/CMakeLists.txt:43-76`
    - `apps/runtime-service/runtime/configuration/ConfigFileAdapter.h:30-99`
    - `apps/runtime-service/runtime/configuration/ConfigFileAdapter.cpp:48-127`
  - 为什么这是结构问题而不是局部实现问题：
    - owner root 与 live implementation root 脱节，违背仓库分层原则。
  - 可能后果：
    - app 层成为事实 owner。
    - 模块无法独立演进。
  - 优先级：P0

- 问题名称：配置端口混装 M1/M2/M9 职责
  - 现象：
    - 同一模块同时承载文件存储、DXF 预处理、机器/回零、硬件模式、诊断、备份恢复。
  - 涉及文件：
    - `modules/process-planning/domain/configuration/ports/IFileStoragePort.h:22-91`
    - `modules/process-planning/domain/configuration/ports/IConfigurationPort.h:256-401`
  - 为什么这是结构问题而不是局部实现问题：
    - 这是 owner 语义混杂，导致模块边界天然不可稳定。
  - 可能后果：
    - 任一配置改动都会跨文件接入、几何预处理、运行时一起扩散。
  - 优先级：P1

- 问题名称：逆向 concrete 依赖和隐藏跨模块依赖
  - 现象：
    - M1/M2 直接依赖 `process-planning/domain/configuration`。
    - M4 依赖 workflow motion/safety 类型，但 target 未声明，只靠根级全局 include。
  - 涉及文件：
    - `modules/job-ingest/CMakeLists.txt:39-42`
    - `modules/dxf-geometry/CMakeLists.txt:25-27`
    - `modules/process-planning/domain/configuration/value-objects/ConfigTypes.h:3-5`
    - `modules/process-planning/domain/configuration/CMakeLists.txt:11-19`
    - `CMakeLists.txt:386-396`
  - 为什么这是结构问题而不是局部实现问题：
    - 这是依赖图和构建图的系统性偏移。
  - 可能后果：
    - 迁移 bridge 或拆模块时出现大面积连锁破坏。
  - 优先级：P1

- 问题名称：workflow 残留重复配置模型
  - 现象：
    - workflow 保留 `domain/configuration` README、ConfigTypes 和 bridge header。
    - 与 process-planning 形成双份配置模型。
  - 涉及文件：
    - `modules/workflow/domain/domain/configuration/README.md:1-54`
    - `modules/workflow/domain/domain/configuration/value-objects/ConfigTypes.h:1-430`
    - `modules/workflow/domain/include/domain/configuration/ports/IConfigurationPort.h:1-3`
  - 为什么这是结构问题而不是局部实现问题：
    - owner 迁移未完成，重复模型会长期并存。
  - 可能后果：
    - 类型演进和 bridge 退出都需要双边同步。
  - 优先级：P2

## 6. 模块结论

- 宣称职责：
  - 全局文档：`FeatureGraph -> ProcessPlan`
  - 模块本地文档：configuration owner
- 实际职责：
  - 跨链配置模型与兼容桥接，不是工艺规划与工艺决策
- 是否职责单一：否
- 是否边界清晰：否
- 是否被侵入：是
- 是否侵入别人：是
- 是否适合作为稳定业务模块继续演进：不适合
- 最终评级：高风险

## 7. 修复顺序

### 第一步：先收口模块角色

- 目标：
  - 统一 M4 的单一口径。
  - 若保持模块名与冻结文档不变，则 M4 必须回到 `ProcessPlan` owner。
  - 当前 configuration 面应被降级为迁移残留，而不是继续冒充 M4 全貌。
- 涉及目录/文件：
  - `modules/process-planning/README.md`
  - `modules/process-planning/module.yaml`
  - `modules/process-planning/contracts/README.md`
  - `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s05-module-boundary-interface-contract.md`
  - `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s06-repo-structure-guide.md`
- 收益：
  - 先消除“全局文档一套、模块本地一套”的根本冲突。
- 风险：
  - 会立即暴露当前 S5 owner 缺位问题。

### 第二步：再迁移当前配置事实

- 目标：
  - 把 `IFileStoragePort` 迁到 M1 或共享基础设施。
  - 把 `DxfPreprocessConfig` 迁到 M2。
  - 把 machine/homing/hardware/diagnostics/backup 配置迁到 M9 或独立 runtime-config 模块。
  - 把 `ConfigFileAdapter` 从 `apps/runtime-service/` 迁回对应 owner。
- 涉及目录/文件：
  - `modules/process-planning/domain/configuration/ports/`
  - `apps/runtime-service/runtime/configuration/`
  - `modules/job-ingest/`
  - `modules/dxf-geometry/`
  - `modules/runtime-execution/`
- 收益：
  - 切断 M1/M2 对 M4 concrete 的逆向依赖。
  - 消除 app 层 owner 漂移。
- 风险：
  - 构造注入、include 路径、target link 会有较大调整。

### 第三步：最后统一真正的 M4

- 目标：
  - 在 `modules/process-planning/` 内补 `ProcessPlan` contracts/domain/application。
  - 让 `workflow` 在进入 M5-M8 之前，先显式调用 M4。
  - 删除 workflow 重复配置模型与 legacy bridge。
  - 删除根级全局 include 对隐藏依赖的兜底。
- 涉及目录/文件：
  - `modules/process-planning/`
  - `modules/workflow/application/include/application/usecases/dispensing/PlanningUseCase.h`
  - `modules/workflow/domain/domain/configuration/`
  - `modules/workflow/domain/include/domain/configuration/`
  - `CMakeLists.txt`
- 收益：
  - 恢复业务链语义和长期演进边界。
- 风险：
  - 会触及 workflow 编排和现有兼容调用链。

## 8. 证据索引

| 文件路径 | 得出的判断 | 支撑结论 |
|---|---|---|
| `modules/process-planning/README.md` | 模块本地口径已收缩为 configuration owner | M4 角色漂移 |
| `modules/process-planning/module.yaml` | owner_artifact 已变成 `PlanningConfiguration` | 模块实际不再是 `ProcessPlan` owner |
| `modules/process-planning/CMakeLists.txt` | 模块根 target 只接 configuration 子域 | 构建组织不支持工艺规划定位 |
| `modules/process-planning/application/include/process_planning/application/ProcessPlanningApplicationSurface.h` | public surface 只是转发配置契约 | 模块 public face 是空壳 |
| `modules/process-planning/domain/configuration/CMPPulseConfig.cpp` | live source 为占位文件 | 模块内部没有真实 owner 实现 |
| `modules/process-planning/domain/configuration/DispensingConfig.cpp` | live source 为占位文件 | 模块内部没有真实 owner 实现 |
| `modules/process-planning/domain/configuration/InterpolationConfig.cpp` | live source 为占位文件 | 模块内部没有真实 owner 实现 |
| `modules/process-planning/domain/configuration/MachineConfig.cpp` | live source 为占位文件 | 模块内部没有真实 owner 实现 |
| `modules/process-planning/domain/configuration/ValveTimingConfig.cpp` | live source 为占位文件 | 模块内部没有真实 owner 实现 |
| `modules/process-planning/domain/configuration/ports/IConfigurationPort.h` | 配置端口混装 DXF、机器、回零、诊断、硬件、备份恢复 | 职责混杂、侵入其它模块 |
| `modules/process-planning/domain/configuration/ports/IFileStoragePort.h` | 文件存储抽象被放进 M4 | 侵入 M1 文件接入职责 |
| `apps/runtime-service/runtime/configuration/ConfigFileAdapter.cpp` | 实际配置读取/持久化位于 app 层 | owner 实现外置 |
| `apps/runtime-service/runtime/configuration/ConfigFileAdapter.Sections.cpp` | 实际分段加载与配置校验位于 app 层 | owner 实现外置 |
| `modules/job-ingest/CMakeLists.txt` | M1 直接链接 M4 concrete domain target | 逆向依赖 |
| `modules/job-ingest/application/usecases/dispensing/UploadFileUseCase.h` | M1 直接吃 M4 的文件存储与配置端口 | M4 侵入 M1 |
| `modules/job-ingest/application/usecases/dispensing/UploadFileUseCase.cpp` | M1 直接吃 M4 的文件存储与配置端口 | M4 侵入 M1 |
| `modules/dxf-geometry/CMakeLists.txt` | M2 直接链接 M4 concrete domain target | 逆向依赖 |
| `modules/dxf-geometry/application/services/dxf/DxfPbPreparationService.cpp` | M2 通过 M4 读取 DXF 预处理配置 | M4 侵入 M2 |
| `modules/workflow/application/include/application/usecases/dispensing/PlanningUseCase.h` | workflow 直接编排 M6/M7/M8，没有显式 M4 ProcessPlan 阶段 | M4 owner 缺位/被绕过 |
| `modules/process-planning/domain/configuration/value-objects/ConfigTypes.h` | M4 直接包含 workflow motion/safety 类型 | 隐藏跨模块依赖 |
| `modules/workflow/domain/domain/configuration/value-objects/ConfigTypes.h` | workflow 保留重复配置模型 | 重复建模 |
| `modules/workflow/domain/include/domain/configuration/ports/IConfigurationPort.h` | workflow 仍依赖 process-planning legacy bridge 头 | bridge 未退出 |
| `CMakeLists.txt` | 根构建通过全局 include 注入 workflow 头目录 | 真实依赖被掩盖 |

## 9. 复核附录

- 2026-03-31 二次复核对 `ConfigTypes.h` 做了 `git diff --no-index`，结果只看到 include 路径差异：

```diff
#include "domain/motion/value-objects/MotionTypes.h"
#include "domain/safety/value-objects/InterlockTypes.h"
-->
#include "../../motion/value-objects/MotionTypes.h"
#include "../../safety/value-objects/InterlockTypes.h"
```

- 这支持正文中的判断：`modules/process-planning/domain/configuration/value-objects/ConfigTypes.h` 与 `modules/workflow/domain/domain/configuration/value-objects/ConfigTypes.h` 仍是几乎同一套配置模型，当前可见差异仅是 include 路径。
