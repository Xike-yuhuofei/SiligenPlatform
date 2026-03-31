# process-path 模块结构审查

- 审查时间：`2026-03-31 07:48:44 +08:00`
- 审查对象：`modules/process-path/`
- 审查方式：先读声明性边界，再读实现；仅查看判断边界所必需的少量关联代码
- 最终评级：`高风险`

## 1. 模块定位

- 从文档和代码推断，`process-path` 的核心职责是把上游给出的 `Primitive` 集合收敛为 `NormalizedPath -> ProcessPath -> shaped ProcessPath`，即“几何规范化、工艺标注、路径整形”这一段。
  证据：`modules/process-path/README.md:7`、`modules/process-path/README.md:10`、`modules/process-path/application/services/process_path/ProcessPathFacade.cpp:9`
- 它在业务链中的实际位置，落在“路径源装载/偏移之后，运动规划之前”。
  证据：`modules/workflow/application/usecases/dispensing/PlanningUseCase.cpp:508`、`modules/workflow/application/usecases/dispensing/PlanningUseCase.cpp:531`、`modules/workflow/application/usecases/dispensing/PlanningUseCase.cpp:534`
- 它的主要输入，按契约声明包括 `primitives`、`NormalizationConfig`、`ProcessConfig`、`TrajectoryShaperConfig`，以及可选 `CoordinateTransformSet`。
  证据：`modules/process-path/contracts/include/process_path/contracts/PathGenerationRequest.h:13`
- 它的主要输出，是 `normalized`、`process_path`、`shaped_path` 三个结果对象。
  证据：`modules/process-path/contracts/include/process_path/contracts/PathGenerationResult.h:8`
- 它应当依赖 `M5 coordinate-alignment` 的稳定契约、`shared` 的基础类型/几何工具，以及上游提供的 primitive 事实；不应依赖 `workflow` 的构建上下文或历史 bridge。
- 应当依赖它的，是 `workflow` 这类编排层、`runtime-service` 这类宿主装配层，以及 `motion-planning` 这类下游正式消费者；下游应通过 `process_path/contracts` 或 facade 消费，而不是直连它的 `domain` 内部头。
  证据：`apps/runtime-service/container/ApplicationContainer.Dispensing.cpp:78`、`modules/motion-planning/contracts/include/motion_planning/contracts/MotionPlanningRequest.h:8`

## 2. 声明边界 vs 实际实现

- 仓库文档里，`process-path` 被定义为 `M6 ProcessPath` owner，只负责路径事实、规范化、标注、整形，不再承载 motion 语义，并宣称 `ProcessPathFacade` 是唯一 public entry。
  证据：`modules/process-path/README.md:3`、`modules/process-path/README.md:11`、`modules/process-path/module.yaml:3`
- 但仓库总览文档仍把 `MotionPlanner.cpp`、`MotionConfig.h`、`Trajectory.h` 等 motion 资产写在 `process-path` 下，这是过期声明，和当前目录实际内容不一致。
  证据：`modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md:704`
- 实际代码里，live facade 只做三步：`GeometryNormalizer`、`ProcessAnnotator`、`TrajectoryShaper`。这和“它不再承载 motion”是一致的。
  证据：`modules/process-path/application/services/process_path/ProcessPathFacade.cpp:10`

### 偏差点

- 偏差一：文档宣称“基于 `CoordinateTransformSet` 构建 `ProcessPath`”，但 live 链既没有给 `process-path` 传 `alignment`，模块内部也没有消费它。
  证据：`modules/process-path/README.md:7`、`modules/process-path/contracts/include/process_path/contracts/PathGenerationRequest.h:15`、`modules/process-path/application/services/process_path/ProcessPathFacade.cpp:9`、`modules/workflow/application/usecases/dispensing/PlanningUseCase.cpp:275`
- 偏差二：文档宣称 facade/request/result 是唯一 public surface，但 `contracts` 头文件本质上只是对 `domain` 头的相对路径别名，模块根 target 也允许退回到 `siligen_process_path_domain_trajectory`。这说明 public surface 没有真正从内部实现分离出来。
  证据：`modules/process-path/contracts/include/process_path/contracts/ProcessPath.h:3`、`modules/process-path/contracts/include/process_path/contracts/NormalizationConfig.h:3`、`modules/process-path/CMakeLists.txt:27`
- 偏差三：存在“该模块只是目录存在，但真实实现散落在外部”的情况。`process-path/adapters/` 是空占位，但 `IPathSourcePort/IDXFPathSourcePort` 的实现放在 `workflow/adapters`，并且同一套实现又在 `dxf-geometry/adapters/dxf` 下完整重复一份。
  证据：`modules/process-path/adapters/README.md:3`、`modules/workflow/adapters/include/infrastructure/adapters/planning/dxf/AutoPathSourceAdapter.h:24`、`modules/dxf-geometry/adapters/dxf/AutoPathSourceAdapter.h:24`
- 偏差四：存在“该模块应有实现落在别处”的情况。`workflow/domain/domain/trajectory` 仍保留并编译 `GeometryNormalizer/ProcessAnnotator/TrajectoryShaper/PathRegularizer` 的 owner 副本；这些文件与 `modules/process-path/domain/trajectory` 下对应文件内容一致，不是 thin bridge。
  证据：`modules/workflow/domain/domain/trajectory/CMakeLists.txt:3`、`modules/process-path/domain/trajectory/CMakeLists.txt:8`

### 重点检查结论

- 存在该模块应有实现却落在别处：是，`workflow/domain/domain/trajectory` 持续保留并编译 `M6` 领域实现副本。
- 存在别的模块的 owner 事实被放进该模块：有残留迹象，`PathRegularizer` 和 `IDXFPathSourcePort` 明显带有 DXF/file-ingest 色彩，但是否最终应属于 `M2` 还是 `M6`，证据不足。
- 存在该模块只是“目录存在”，但真实实现散落在外部：是，适配器实现明确散落在 `workflow` 和 `dxf-geometry`。
- 存在与其它模块重复建模、重复流程、重复数据定义：是，`workflow/domain/domain/trajectory` 与 `modules/process-path/domain/trajectory` 重复；`workflow/adapters/planning/dxf` 与 `dxf-geometry/adapters/dxf` 重复。

## 3. 模块内部结构审查

- 内部结构：`一般`

### 原因

- 原因一：live 实现本身不混乱。真正有行为的代码集中在 `domain/trajectory` 和一个 facade，领域算法与应用编排基本分开。
  证据：`modules/process-path/domain/trajectory/CMakeLists.txt:8`、`modules/process-path/application/services/process_path/ProcessPathFacade.cpp:9`
- 原因二：公共接口与内部实现没有真正分离。`contracts` 直接 `#include ../../../../domain/...`，这是“公开头对内部头的别名层”，不是独立契约层。
  证据：`modules/process-path/contracts/include/process_path/contracts/Path.h:3`、`modules/process-path/contracts/include/process_path/contracts/ProcessPath.h:3`、`modules/process-path/contracts/include/process_path/contracts/TrajectoryShaperConfig.h:3`
- 原因三：目录骨架与真实实现不一致。`services/`、`adapters/`、`examples/` 大多是占位，真实适配器和一部分 owner 行为散落在 `workflow` 与 `dxf-geometry`。
  证据：`modules/process-path/services/README.md:3`、`modules/process-path/services/process/README.md:5`、`modules/process-path/adapters/README.md:3`
- 原因四：存在层次混杂。`PathRegularizer` 和 `IDXFPathSourcePort` 带有 DXF 输入/文件格式处理色彩，却放在 `domain/trajectory` 内；这不是纯粹的 `ProcessPath` 语义层。
  证据：`modules/process-path/domain/trajectory/domain-services/PathRegularizer.h:14`、`modules/process-path/domain/trajectory/ports/IDXFPathSourcePort.h:13`
- 原因五：CMake 入口没有完全反映“唯一 public entry”。模块根 target 和 `workflow` 都保留了向 `siligen_process_path_domain_trajectory` 退回的路径。
  证据：`modules/process-path/CMakeLists.txt:27`、`modules/workflow/CMakeLists.txt:216`

## 4. 对外依赖与被依赖关系

### 合理依赖

- `shared/types`、`shared/Geometry`、`shared compat include` 是合理依赖，支撑 `Primitive/Path/ProcessPath` 和几何计算。
  证据：`modules/process-path/domain/trajectory/value-objects/Primitive.h:3`、`modules/process-path/domain/trajectory/domain-services/PathRegularizer.cpp:4`、`modules/process-path/domain/trajectory/CMakeLists.txt:30`
- `motion-planning` 通过 `process_path/contracts/ProcessPath.h` 消费 `ProcessPath`，依赖方向合理。
  证据：`modules/motion-planning/contracts/include/motion_planning/contracts/MotionPlanningRequest.h:4`

### 可疑依赖

- `coordinate-alignment/contracts` 在语义上合理，但当前是“死缝”。请求契约依赖了它，模块实现和 live 调用链都没用到它。
  证据：`modules/process-path/contracts/include/process_path/contracts/PathGenerationRequest.h:3`、`modules/workflow/application/usecases/dispensing/PlanningUseCase.cpp:275`
- `DXFSegment`、`DXFValidationResult`、文件加载/预处理端口放在 `process-path/domain` 下，说明模块仍承载上游输入边界残留；这对“工艺路径语义模块”而言是可疑依赖方向。
  证据：`modules/process-path/domain/trajectory/ports/IPathSourcePort.h:17`、`modules/process-path/domain/trajectory/ports/IDXFPathSourcePort.h:19`、`modules/process-path/domain/trajectory/domain-services/PathRegularizer.h:32`

### 高风险依赖

- `process-path/application` 和 `tests` 直接依赖 `workflow` 提供的 `PROCESS_RUNTIME_CORE_*` 变量、`siligen_process_runtime_core_boost_headers` target、`process_runtime_core_add_test_with_runtime_environment` 函数；这是配置期反向依赖。
  证据：`modules/process-path/application/CMakeLists.txt:28`、`modules/process-path/domain/trajectory/CMakeLists.txt:30`、`modules/process-path/tests/CMakeLists.txt:30`、`modules/workflow/CMakeLists.txt:53`
- `workflow` 直接编译/导出 `siligen_process_path_domain_trajectory`，并保留同内容副本；这是 owner 反向侵入。
  证据：`modules/workflow/domain/CMakeLists.txt:57`、`modules/workflow/domain/domain/CMakeLists.txt:107`
- `dispense-packaging` 仍通过 `domain/trajectory/value-objects/ProcessPath.h` 直连 M6 内部类型，同时又混用 `process_path/contracts/GeometryUtils.h`；这是跨层直连。
  证据：`modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.h:8`、`modules/dispense-packaging/domain/dispensing/planning/domain-services/AuthorityTriggerLayoutPlanner.h:5`、`modules/dispense-packaging/domain/dispensing/planning/domain-services/AuthorityTriggerLayoutPlanner.cpp:7`

### 工程后果

- 未见明确的链接环依赖证据。
- 已见明确的配置期反向依赖、跨层直连、owner 副本重复编译、输入适配实现双处重复。
- 长期后果是：边界无法稳定、修复会双改、任何 `ProcessPath` 结构演进都会牵连 `workflow`、`dispense-packaging`、`dxf-geometry` 多处一起改。

## 5. 结构性问题清单

### 1. `workflow` 持续编译并保留 `M6` owner 领域副本

- 现象：`workflow/domain/domain/trajectory` 仍编译 `GeometryNormalizer`、`PathRegularizer`、`ProcessAnnotator`、`TrajectoryShaper`，且与 `modules/process-path/domain/trajectory` 对应文件内容一致，不是薄桥接。
- 涉及文件：`modules/workflow/domain/domain/trajectory/CMakeLists.txt:3`、`modules/process-path/domain/trajectory/CMakeLists.txt:8`、`modules/workflow/domain/CMakeLists.txt:57`
- 为什么这是结构问题而不是局部实现问题：它破坏的是 owner 唯一性，而不是某个函数写法。
- 可能后果：同一逻辑双改、修复漂移、测试结果与 live 链脱节、`M6` 无法成为稳定 owner。
- 优先级：`P0`

### 2. `process-path` 的 public surface 没有真正建立

- 现象：`contracts` 直接别名到 `domain` 头，模块根 target 和 `workflow` 还允许退回到 `siligen_process_path_domain_trajectory`，下游因此继续直连内部类型。
- 涉及文件：`modules/process-path/README.md:11`、`modules/process-path/contracts/include/process_path/contracts/ProcessPath.h:3`、`modules/process-path/CMakeLists.txt:27`、`modules/workflow/CMakeLists.txt:216`、`modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.h:8`
- 为什么这是结构问题而不是局部实现问题：它决定了整个模块能否独立演进，而不是单个 API 是否好看。
- 可能后果：一旦 `ProcessPath` 结构或算法分层调整，所有下游都被迫跟着内部实现细节一起改。
- 优先级：`P1`

### 3. 输入边界和适配实现未收口，且在外部重复存在

- 现象：`process-path/adapters` 为空，占位文档要求适配器收敛到本模块；但 `IPathSourcePort/IDXFPathSourcePort` 的实现同时存在于 `workflow/adapters` 和 `dxf-geometry/adapters/dxf`，并且文件内容重复。
- 涉及文件：`modules/process-path/adapters/README.md:3`、`modules/process-path/domain/trajectory/ports/IPathSourcePort.h:31`、`modules/process-path/domain/trajectory/ports/IDXFPathSourcePort.h:36`、`modules/workflow/adapters/include/infrastructure/adapters/planning/dxf/AutoPathSourceAdapter.h:24`、`modules/dxf-geometry/adapters/dxf/AutoPathSourceAdapter.h:24`
- 为什么这是结构问题而不是局部实现问题：这里失去的是“单一输入 owner”，不是某个 adapter 的实现细节。
- 可能后果：DXF/PB 输入链路无法唯一追责，任何输入格式修复都可能三处同时改、三处不一致。
- 优先级：`P1`

### 4. 声明的 `M5 -> M6` 对齐缝是死缝

- 现象：`PathGenerationRequest` 暴露 `alignment`，README 说基于 `CoordinateTransformSet` 构建 `ProcessPath`；但 facade 不使用该字段，`workflow` live 链也不填该字段，而是先把 offset 直接作用在 primitives 上。
- 涉及文件：`modules/process-path/README.md:7`、`modules/process-path/contracts/include/process_path/contracts/PathGenerationRequest.h:15`、`modules/process-path/application/services/process_path/ProcessPathFacade.cpp:15`、`modules/workflow/application/usecases/dispensing/PlanningUseCase.cpp:263`、`modules/workflow/application/usecases/dispensing/PlanningUseCase.cpp:279`
- 为什么这是结构问题而不是局部实现问题：这是上游模块间契约是否真实生效的问题，不是某个参数漏传的小 bug。
- 可能后果：后续一旦真要引入对齐语义，只能继续在 `workflow` 里补丁式前置变换，`M6` 仍然拿不到正式 `M5` 事实。
- 优先级：`P1`

### 5. 模块构建边界依赖 `workflow` 全局上下文

- 现象：`process-path` 的 application、domain、tests 直接依赖 `PROCESS_RUNTIME_CORE_*`、`siligen_process_runtime_core_boost_headers`、`process_runtime_core_add_test_with_runtime_environment`；这些都由 `workflow` 定义。
- 涉及文件：`modules/process-path/application/CMakeLists.txt:28`、`modules/process-path/domain/trajectory/CMakeLists.txt:30`、`modules/process-path/tests/CMakeLists.txt:30`、`modules/workflow/CMakeLists.txt:53`
- 为什么这是结构问题而不是局部实现问题：这影响的是模块是否能作为独立 owner 被组装、裁剪和验证。
- 可能后果：模块无法独立验证，构建顺序稍变就可能失效，owner 模块名义独立、实则受制于历史 super-context。
- 优先级：`P2`

## 6. 模块结论

- 宣称职责：基于上游规则与 `CoordinateTransformSet` 生成 `ProcessPath`，并负责规范化、工艺标注、路径整形，不承载 motion 语义。
- 实际职责：live 上只负责 `Primitive -> NormalizedPath -> ProcessPath -> shaped ProcessPath`；同时残留 DXF/file-ingest 端口与 `PathRegularizer`，且输入适配与部分 owner 实现散落在外部模块。
- 是否职责单一：否。
- 是否边界清晰：否。
- 是否被侵入：是，尤其被 `workflow` 的 owner 副本和下游直连内部类型侵入。
- 是否侵入别人：是，至少仍承载了 DXF 输入/预处理边界残留；最终 owner 归属证据不足，但可以明确不是纯 `ProcessPath` 语义面。
- 是否适合作为稳定业务模块继续演进：不适合，先收口边界再演进。
- 最终评级：`高风险`

## 7. 修复顺序

### 1. 先收口 owner 唯一性

- 目标：让 `M6` 的领域实现只在 `modules/process-path/` 一处存在，`workflow` 只保留 facade 消费与必要 bridge header。
- 涉及目录/文件：`modules/workflow/domain/domain/trajectory/`、`modules/workflow/domain/CMakeLists.txt`、`modules/workflow/domain/domain/CMakeLists.txt`、`modules/workflow/tests/process-runtime-core/unit/domain/trajectory/`
- 收益：先把最危险的双份 owner 消掉，后续所有边界修复才有单一落点。
- 风险：会牵动历史兼容测试和旧 include，需要先识别仍在依赖副本的调用链。

### 2. 再迁移输入边界

- 目标：为 `IPathSourcePort/IDXFPathSourcePort` 和 `PathRegularizer` 选定唯一 owner；如果它们属于 DXF 输入链，就收敛到 `dxf-geometry`，如果属于 `M6`，就把实现迁回 `modules/process-path/adapters/` 并纳入正式入口。
- 涉及目录/文件：`modules/process-path/domain/trajectory/ports/`、`modules/process-path/domain/trajectory/domain-services/PathRegularizer.*`、`modules/process-path/adapters/`、`modules/workflow/adapters/infrastructure/adapters/planning/dxf/`、`modules/dxf-geometry/adapters/dxf/`
- 收益：消除“目录在 M6、实现在 workflow/dxf-geometry、而且两边重复”的三头结构。
- 风险：会触及 DXF/PB 输入链和迁移期兼容分支，短期要补桥接。

### 3. 最后统一 public surface、对齐缝和构建组织

- 目标：让 `process_path/contracts` 成为真正稳定契约；下游只经 contracts/facade 使用；决定 `alignment` 是删除还是正式接入；去掉对 `workflow` 全局 CMake 上下文的依赖。
- 涉及目录/文件：`modules/process-path/contracts/include/process_path/contracts/`、`modules/process-path/CMakeLists.txt`、`modules/process-path/application/CMakeLists.txt`、`modules/process-path/tests/CMakeLists.txt`、`modules/workflow/application/usecases/dispensing/PlanningUseCase.cpp`
- 收益：文档、契约、实现、构建边界一致，`M6` 才能稳定演进。
- 风险：会引起下游 include/target 切换，短期改动面较广。

## 8. 证据索引

| 文件路径 | 得出的判断 | 支撑结论 |
|---|---|---|
| `modules/process-path/README.md` | 文档把 `M6` 定义为 `ProcessPath` owner，且宣称 facade 是唯一 public entry，同时仍提到 `CoordinateTransformSet` | 模块定位；声明边界与实际不一致 |
| `modules/process-path/application/services/process_path/ProcessPathFacade.cpp` | live facade 只串 `GeometryNormalizer -> ProcessAnnotator -> TrajectoryShaper`，未消费 `alignment` | 实际职责；对齐缝失效 |
| `modules/process-path/contracts/include/process_path/contracts/PathGenerationRequest.h` | 契约暴露 `CoordinateTransformSet`，但只是声明字段 | 声明职责与实际实现偏差 |
| `modules/workflow/application/usecases/dispensing/PlanningUseCase.cpp` | live 编排链先装载 primitives、再调 facade、再调 motion；构造请求时未设置 `alignment` | 模块在业务链中的实际位置；对齐缝失效 |
| `modules/process-path/contracts/include/process_path/contracts/ProcessPath.h` | `contracts` 直接相对包含 `domain` 头，不是独立契约 | public surface 未建立 |
| `modules/process-path/CMakeLists.txt` | 模块根 target 允许退回内部 domain target | 唯一 public entry 未被构建强制执行 |
| `modules/process-path/application/CMakeLists.txt` | application 直接使用 `PROCESS_RUNTIME_CORE_*` 全局变量 | 构建边界依赖 workflow 上下文 |
| `modules/process-path/domain/trajectory/CMakeLists.txt` | domain target 直接依赖 `siligen_process_runtime_core_boost_headers` | 构建自洽性不足 |
| `modules/workflow/domain/CMakeLists.txt` | `workflow` 主动 `add_subdirectory` 引入 `process-path/domain/trajectory` | 反向构建依赖；owner 被侵入 |
| `modules/workflow/domain/domain/trajectory/CMakeLists.txt` | `workflow` 仍编译完整 trajectory 实现副本 | `M6` owner 唯一性被破坏 |
| `modules/workflow/domain/include/domain/trajectory/value-objects/ProcessPath.h` | `workflow` public header 只是桥接到 `process-path` domain 头 | 下游仍能绕过正式 contracts 直连内部类型 |
| `modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.h` | 下游模块直接 include `domain/trajectory/value-objects/ProcessPath.h` | M6 被下游内部直连侵入 |
| `modules/process-path/adapters/README.md` | 文档要求适配器收敛到本模块，但目录为空 | owner 目录存在、实现散落在外 |
| `modules/workflow/adapters/include/infrastructure/adapters/planning/dxf/AutoPathSourceAdapter.h` | `workflow` 实现了 `IPathSourcePort/IDXFPathSourcePort` | 输入边界实现在外部模块 |
| `modules/dxf-geometry/adapters/dxf/AutoPathSourceAdapter.h` | 同名适配器又在 `dxf-geometry` 出现，且与 workflow 版本同内容 | 输入边界重复实现 |
| `modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md` | `process-path` 总览仍描述旧的 motion 资产与过时文件树 | 声明性文档已经漂移，不能准确反映实际边界 |
