# coordinate-alignment Residue Ledger

## 1. Executive Verdict
- 一句话判定：canonical 化进行中但 closeout 未完成。

## 2. Scope
- 本轮只审查的目录范围：`modules/coordinate-alignment/**`
- 为判断边界引用的少量关联对象范围：
  - `modules/process-path/README.md`
  - `modules/process-path/module.yaml`
  - `modules/process-path/contracts/include/process_path/contracts/PathGenerationRequest.h`
  - `modules/workflow/domain/README.md`
  - `modules/workflow/domain/CMakeLists.txt`
  - `modules/workflow/domain/include/domain/machine/**`
  - `modules/runtime-execution/runtime/host/CMakeLists.txt`
  - `modules/runtime-execution/runtime/host/runtime/system/DispenserModelMachineExecutionStateBackend.h`
  - `modules/runtime-execution/adapters/device/CMakeLists.txt`
  - `modules/runtime-execution/adapters/device/src/adapters/**/{HardwareConnectionAdapter.h,HardwareTestAdapter.h,TriggerControllerAdapter.h,MotionRuntimeFacade.h}`
  - `tests/CMakeLists.txt`
  - `tests/contracts/test_bridge_exit_contract.py`

## 3. Baseline Freeze
- owner 基线：
  - `modules/coordinate-alignment/README.md` 将本模块冻结为 `M5 coordinate-alignment` 的 canonical owner，职责是“坐标系基准对齐语义”“坐标变换与对齐参数”“面向 M6 process-path 的对齐结果口径”。
  - `modules/coordinate-alignment/module.yaml` 冻结 `owner_artifact: CoordinateTransformSet`、`group: planning`，并把模块允许依赖限制为 `process-planning/contracts` 与 `shared`。
- 职责边界基线：
  - `README.md` 明确 `workflow`、`process-planning` 只提供上游输入，不承载 `M5` 终态事实。
  - `README.md` 明确运行时入口与执行链不得回写 `M5` owner 事实。
  - `README.md` 与 `contracts/README.md` 一致要求：跨模块长期稳定契约应落在 `shared/contracts/engineering/`，本模块只保留 `M5` 专属契约。
- public surface 基线：
  - 当前唯一清晰、可被下游直接消费的稳定 seam 是 `contracts/include/coordinate_alignment/contracts/CoordinateTransformSet.h`。
  - `modules/process-path/contracts/include/process_path/contracts/PathGenerationRequest.h` 只依赖 `coordinate_alignment/contracts/CoordinateTransformSet.h`，没有依赖 `domain/machine/**` 或 application façade。
  - `application/include/coordinate_alignment/application/CoordinateAlignmentApplicationSurface.h` 目前只是对 contracts header 的单行转发，不足以构成真实 application seam。
- build 基线：
  - 模块根 target 是 `siligen_module_coordinate_alignment`。
  - 从 frozen owner 视角，根 target 应优先暴露 `CoordinateTransformSet` 或真实 M5 façade，而不是 machine/runtime concrete。
  - `module.yaml` 冻结的 allowed dependencies 不包含 `workflow` include root、`trace-diagnostics/contracts`、`domain/motion` 或 runtime/hardware concrete。
- test 基线：
  - `tests/CMakeLists.txt` 是模块级正式验证入口，并由仓库级 `tests/CMakeLists.txt` 注册为 canonical test root。
  - `tests/README.md` 自证当前测试面分为 `unit/CalibrationProcess`、`contract/CoordinateTransformSet`、`golden/CoordinateTransformSet`，同时承认“真实数值型 origin offset / rotation 补偿字段契约”和 `coordinate-alignment -> process-path` integration 尚未补齐。

## 4. Top-Level Findings
- `CoordinateTransformSet` 是 frozen owner artifact，也是 `process-path` 唯一实际消费的 seam；但模块内 live implementation 几乎不围绕它展开，`domain/services/adapters` 下不存在对应 producer 实现。
- `domain/machine/` 当前承载的是点胶机状态、任务队列、标定流程、硬件连接、硬件测试等 machine/runtime concrete，不是 M5 对齐事实 owner。
- `siligen_coordinate_alignment_domain_machine` 是 live build target，而且被 `siligen_module_coordinate_alignment`、`workflow/domain` 桥和 `runtime-execution/runtime/host`、`runtime-execution/adapters/device` 直接消费，因此这些 residual 不是死代码。
- 模块根 target 与 `application_public` 都在公开转发 `domain/machine` concrete，导致 internal stage 类型通过模块公共面外泄。
- `module.yaml` 冻结的 allowed dependencies 与实际依赖图冲突：当前 live target 直接链接 `trace-diagnostics/contracts`、公开引入 workflow domain include root，并通过 `IHardwareTestPort` 绑定 motion 相关类型。
- `IHardwareConnectionPort` 与 `IHardwareTestPort` 是两个活跃的 residual 容器：前者承载连接/心跳 runtime compat，后者把 jog/homing/IO/trigger/CMP/interpolation/diagnostics/safety 混在 M5 下。
- `CoordinateAlignmentApplicationSurface.h` 看起来像 canonical façade，实际上只是 contract umbrella；repo 内唯一 consumer 还是本模块自己的 contract test。
- 当前测试 gate 更像在证明 `CalibrationProcess` 还能跑，而不是在证明 `CoordinateTransformSet` 足以作为 M5 对齐 owner closeout 的正式产物。

## 5. Residue Ledger Summary
- owner residue：4
- build residue：2
- surface residue：2
- naming residue：1
- test residue：1
- placeholder residue：1

## 6. Residue Ledger Table
| residue_id | severity | file_or_target | current_role | why_residue | violated_baseline | evidence | action_class | preferred_destination | confidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| CA-R001 | P1 | `modules/coordinate-alignment/domain/machine/aggregates/DispenserModel.h` / `.cpp` | live 点胶机状态、任务队列、运动/连接配置与统计聚合；被 runtime host 直接作为 machine execution state model 消费 | 该对象描述的是 dispenser runtime/process-control concrete，而不是 `CoordinateTransformSet` 或对齐参数 owner；它把设备态、任务态和执行期统计长期停放在 M5 | `README.md` 冻结 M5 为对齐语义 owner；运行时不得回写 M5 owner 事实 | `README.md` 的 owner 范围；`domain/machine/README.md` 明确“设备整体状态管理、任务编排、硬件连接管理”；`domain/machine/CMakeLists.txt` 把它编进 live target；`modules/runtime-execution/runtime/host/CMakeLists.txt` 与 `runtime/system/DispenserModelMachineExecutionStateBackend.h` 直接消费它 | migrate | 非 M5 的 machine execution state owner；当前最近的 live owner 落点是 `modules/runtime-execution/runtime/host/runtime/system/` | high |
| CA-R002 | P1 | `modules/coordinate-alignment/domain/machine/domain-services/CalibrationProcess.*`、`value-objects/CalibrationTypes.h`、`ports/ICalibrationDevicePort.h`、`ports/ICalibrationResultPort.h` | live 标定流程状态机、设备/结果端口、模块 unit gate 主证据 | 该族对象承载的是校准流程编排、状态推进和结果持久化接口；它与 `CoordinateTransformSet` 没有 contract-level 对齐，且成为当前模块最强的“可运行证明” | `owner_artifact: CoordinateTransformSet`；M5 应输出对齐事实而不是长期持有流程控制态 | `CoordinateTransformSet.h` 是唯一 frozen owner artifact；`CalibrationProcess.h` 暴露完整状态机；`CalibrationProcessTest.cpp` 是 unit 主证据；`rg` 在 `domain/application/services/adapters` 中对 `CoordinateTransformSet|alignment_id` 的命中只剩 application umbrella | split | 非 M5 的 calibration/process-control owner；若 M5 仍消费其结果，也应只回流对齐事实 DTO | medium |
| CA-R003 | P1 | `modules/coordinate-alignment/domain/machine/ports/IHardwareConnectionPort.h` | live 硬件连接、状态监控、心跳兼容端口；被 runtime device adapters 使用 | 这是 runtime/hardware compat surface，不是 coordinate-alignment owner；它还把心跳、连接状态、设备元信息长期挂在 M5 名下 | `module.yaml` allowed dependencies 不包含 runtime/hardware concrete；README 明确运行时不得回写 M5 owner 事实 | `IHardwareConnectionPort.h` 定义连接配置、心跳配置、心跳状态；`HardwareConnectionAdapter.h` 注释明确“稳定公开面实现 DeviceConnectionPort，旧 IHardwareConnectionPort 仅保留兼容桥”；`MotionRuntimeFacade.h` 继续消费这些类型 | demote | `shared/contracts/device` 的稳定设备契约；任何 compat bridge 也不应继续由 M5 持有 | high |
| CA-R004 | P0 | `modules/coordinate-alignment/domain/machine/ports/IHardwareTestPort.h` | live 巨型测试/硬件控制端口，覆盖 jog、homing、IO、trigger、CMP、interpolation、diagnostics、safety | 单个端口同时混入 motion、diagnostics、dispensing trigger、安全与硬件测试能力，是典型 multi-owner residual 容器；它把 M5 绑到了多个非 owner 语义 | M5 应承载对齐结果，而不是 motion/diagnostics/safety/hardware-test surface；allowed dependencies 也未冻结这些依赖 | `IHardwareTestPort.h` 直接 include `trace_diagnostics/contracts/DiagnosticTypes.h` 与 `domain/motion/value-objects/*`；`HardwareTestAdapter.h` 真实实现它；`TriggerControllerAdapter.h` 又基于它实现 dispensing trigger；`test_bridge_exit_contract.py` 仍显式跟踪该 header 与 workflow 副本 | split | 拆到 `shared/contracts/device`、`modules/runtime-execution/contracts`、`modules/trace-diagnostics/contracts` 等真实 owner surface；遗留 compat 不应留在 M5 | high |
| CA-R005 | P0 | `modules/coordinate-alignment/CMakeLists.txt`、`application/CMakeLists.txt`、`domain/machine/CMakeLists.txt`、target `siligen_module_coordinate_alignment` | 模块根 target 通过 `application_public` 和 `domain_machine` 公开 machine concrete；`domain_machine` 还把 module root 与 workflow include root 一起设成 PUBLIC include | 当前 build graph 让 `domain/machine/**` 成为 M5 live public surface，并把 workflow bridge include root 反向灌入 M5；application 层并未隔离 residual，只是在转发它 | frozen build baseline 应优先暴露 `CoordinateTransformSet` 或真实 façade，而不是 machine concrete；`module.yaml` allowed dependencies 与实际 graph 冲突 | `CMakeLists.txt` 26-39 公开链接 `application_public`/`domain_machine`；`application/CMakeLists.txt` 32-45 再次链接 `domain_machine`；`domain/machine/CMakeLists.txt` 15-25 公开 module root 与 workflow include root，并链接 `trace-diagnostics/contracts`；`runtime/host/CMakeLists.txt` 85-92 以 `LINK_ONLY` 依赖 `siligen_coordinate_alignment_domain_machine` | shrink | 模块根 target 应收缩到 contracts 或真实 application seam；machine target 应迁出 M5 或降为外部 compat | high |
| CA-R006 | P1 | `modules/coordinate-alignment/contracts/include/coordinate_alignment/contracts/CoordinateTransformSet.h` | 当前唯一真实 consumer-facing contract；被 `process-path` 使用 | 该 contract 只携带 `alignment_id`、`plan_id`、`valid`、字符串形式 transform 列表；它没有冻结 README 所声称的“对齐参数/坐标变换参数”，也没有模块内 live producer 与之闭环 | `README.md` 将 M5 定义为“坐标变换与对齐参数”的 owner 边界；`tests/README.md` 也承认数值型补偿字段尚未补齐 | `CoordinateTransformSet.h` 8-20；`coordinate_transform_set.baseline.txt` 只有字符串快照；`tests/README.md` 17-19 明示缺少真实数值型字段；`PathGenerationRequest.h` 18 说明 M6 实际消费的就是这个 DTO | split | 冻结“稳定 consumer DTO”与“实际 alignment fact”边界；若跨模块稳定，再考虑外提到 `shared/contracts/engineering` | medium |
| CA-R007 | P2 | `modules/coordinate-alignment/application/include/coordinate_alignment/application/CoordinateAlignmentApplicationSurface.h`、`application/CMakeLists.txt` | 看起来像 canonical application seam，实际上只是 contract umbrella 并顺带把 `domain_machine` 链出去 | façade / provider / service seam 名称已出现，但没有任何真正 application API；它人为制造出一层“closeout 已完成”的错觉 | frozen public surface 实际仍是 contracts；`process-path` 也只依赖 contracts | `CoordinateAlignmentApplicationSurface.h` 只有一行 include；`application/CMakeLists.txt` 32-45 把 contracts 和 `domain_machine` 一并导出；repo 内对该 header 的唯一命中是 `tests/contract/CoordinateTransformContractsTest.cpp` | demote | 若没有真实 façade，直接回到 contracts seam；若后续建立正式 use case，再重建真实 application surface | high |
| CA-R008 | P1 | `modules/coordinate-alignment/tests/CMakeLists.txt`、`tests/unit/CalibrationProcessTest.cpp`、`tests/contract/CoordinateTransformContractsTest.cpp`、`tests/golden/CoordinateTransformSetGoldenTest.cpp`、`tests/README.md` | 单一模块测试 target 同时承载 calibration unit、contract、golden | 测试组织没有支撑 M5 owner closeout：最强行为证明来自 `CalibrationProcess`，而 contract/golden 只证明字符串字段和快照；缺失数值参数与 `M5 -> M6` 交接验证 | frozen owner artifact 是 `CoordinateTransformSet`；README 声称 M5 结果要供 M6 消费 | `tests/CMakeLists.txt` 3-29；`CalibrationProcessTest.cpp` 1-115；`CoordinateTransformContractsTest.cpp` 与 golden test 都只围绕字符串字段；`tests/README.md` 17-19 明示缺口；仓库级 `tests/CMakeLists.txt` 已把这里注册为 canonical test root | split | contract/golden 留在 M5；calibration tests 跟随真实 owner 迁走；补 `coordinate-alignment -> process-path` integration 与数值契约回归 | high |
| CA-R009 | P2 | `modules/coordinate-alignment/README.md`、`module.yaml`、`domain/README.md`、`domain/machine/README.md`、`tests/README.md` | 文档与命名同时充当基线，但叙述互相打架 | 同一模块被同时描述为“对齐结果 owner”“唯一真实实现是 domain/machine”“设备管理/任务编排/硬件连接 owner”“当前 contract 还没数值字段”，导致 closeout 标准不可执行 | Documentation / Naming 一致性基线 | 根 README 把 M5 定义为 alignment owner；`module.yaml` 只允许 `process-planning/contracts` + `shared`；`domain/machine/README.md` 则声明设备管理与 motion/dispensing 依赖；`tests/README.md` 又承认 contract 仍不完整 | rename | 统一为一份可执行真值：明确 M5 只拥有什么、暂时兼容什么、哪些对象只是残留 | high |
| CA-R010 | P2 | `modules/coordinate-alignment/services/README.md`、`adapters/README.md`、`examples/README.md` | README-only shell 目录，却被模块根 target 标记为 implementation roots | 这些目录没有任何 payload 文件，但文档与 target properties 仍把它们描述成 canonical 承载面，掩盖“真实实现几乎只有 contracts + domain/machine residual”的事实 | `README.md` 的“统一骨架状态”与 build topology 透明性 | `rg --files` 仅返回三个 README；模块根 `CMakeLists.txt` 43-46 仍把 `services;application;adapters;tests;examples` 标为 implementation roots | demote | 从 implementation topology 中降级为 docs-only shell，或在 closeout 时直接删除空壳 | high |
| CA-R011 | P1 | `modules/coordinate-alignment/domain/machine/**` 与 `modules/workflow/domain/include/domain/machine/**` 的组合 seam | machine surface 同时通过 M5 concrete 与 workflow bridge 暴露 | 这形成 dual public seam：一侧是 `coordinate-alignment/domain/machine/**` concrete，一侧是 workflow 的 bridge header；外部 consumer 可以绑定任一侧，导致 owner closeout 无法收口 | 一个模块应只有一个稳定 seam；workflow 自身也声明 `domain_machine` 只是 bridge，不代表 workflow 持有 owner | `modules/workflow/domain/include/domain/machine/aggregates/DispenserModel.h` 与 `domain-services/CalibrationProcess.h` 直接 forward 到 M5；`modules/workflow/domain/CMakeLists.txt` 42-57 把 `domain_machine` 桥到 `siligen_coordinate_alignment_domain_machine`；`modules/workflow/domain/README.md` 7-11 明确 bridge-only；runtime docs 仍引用 workflow `domain/machine/aggregates/DispenserModel.h` 作为 canonical surface | demote | 收敛到单一非 M5 machine owner surface；workflow bridge 只保留短期 compat，随后删除 | high |

## 7. Hotspots
- `modules/coordinate-alignment/domain/machine/CMakeLists.txt`
  - 这是 live residual 的总接线点：编译 `DispenserModel`/`CalibrationProcess`，公开 module root 与 workflow include root，并链接 `trace-diagnostics/contracts`。
- `modules/coordinate-alignment/domain/machine/ports/IHardwareTestPort.h`
  - 单文件聚合 motion、diagnostics、dispensing trigger、安全与硬件测试，是最肥大的 residual 容器。
- `modules/coordinate-alignment/domain/machine/ports/IHardwareConnectionPort.h`
  - 把 runtime connection/heartbeat compat 长期挂在 M5 下，并且存在 live runtime consumers。
- `modules/coordinate-alignment/domain/machine/aggregates/DispenserModel.h`
  - 名称、语义、落点全面偏向 machine execution state，而不是 coordinate alignment。
- `modules/coordinate-alignment/domain/machine/domain-services/CalibrationProcess.h`
  - 它成为当前模块最强的行为性 owner 证据，但 owner artifact 又不是它。
- `modules/coordinate-alignment/CMakeLists.txt`
  - 根 target 当前在帮助 residual 外泄，而不是在收敛 M5 public surface。
- `modules/coordinate-alignment/contracts/include/coordinate_alignment/contracts/CoordinateTransformSet.h`
  - 这是唯一真实 consumer-facing seam，但它的语义壳层过薄，尚不足以支撑 closeout。
- `modules/coordinate-alignment/tests/CMakeLists.txt`
  - 把“CalibrationProcess unit gate”与“CoordinateTransformSet contract/golden”混为一个 closeout gate。

## 8. False Friends
- `siligen_module_coordinate_alignment`
  - 名义上是 M5 根 target，实际上公开转发了 `domain/machine` residual。
- `siligen_coordinate_alignment_application_public`
  - 名义上是 application public surface，实际上只是 contracts + `domain_machine` 的中继层。
- `CoordinateAlignmentApplicationSurface.h`
  - 名义上像 façade，实际上只有单行 include。
- `domain/machine/`
  - 名义上像 M5 的 canonical implementation root，实际上更像 machine/runtime/calibration residual 容器。
- `tests/unit/CalibrationProcessTest.cpp`
  - 名义上像模块 owner 行为证明，实际上证明的是校准流程状态机，而不是 `CoordinateTransformSet`。
- `services/`、`adapters/`、`examples/`
  - 名义上像已占位完成的 canonical roots，实际上仍是无 payload 的壳层。

## 9. Closeout Blockers
- 架构阻塞
  - 必须先冻结：M5 到底只拥有对齐事实，还是长期拥有 calibration/machine concrete；当前证据更支持前者，但 live code 仍按后者布局。
  - `DispenserModel`、`CalibrationProcess`、`IHardwareConnectionPort`、`IHardwareTestPort` 仍作为 live owner/seam 暴露，M5 不能宣称 closeout。
- 构建阻塞
  - `siligen_module_coordinate_alignment` 仍公开引出 `siligen_coordinate_alignment_domain_machine`。
  - `siligen_coordinate_alignment_domain_machine` 仍公开 workflow include root，并被 runtime host/device adapters 直接依赖。
  - `module.yaml` allowed dependencies 与当前 link/include graph 不一致，构建声明不能作为治理真值。
- 测试阻塞
  - 缺少数值型 alignment contract 回归。
  - 缺少 `coordinate-alignment -> process-path` 邻接 integration 证明。
  - Calibration unit tests 仍混在 M5 canonical test target，导致 closeout gate 失焦。
- 文档 / 命名阻塞
  - README、`module.yaml`、`domain/machine/README.md`、tests README 没有形成单一真值。
  - `application surface`、`domain/machine`、`CoordinateTransformSet` 三套命名共同暗示了三种不同 owner 模型。

## 10. No-Change Zone
- `modules/process-path/contracts/include/process_path/contracts/PathGenerationRequest.h`
  - 它是当前唯一已确认的 M5 consumer seam，本轮只把它当边界证据，不扩边改造。
- `modules/workflow/domain/include/domain/machine/**`
  - 这些 bridge header 证明了 dual seam 问题，但本轮不进入 workflow 侧清理。
- `modules/runtime-execution/runtime/host/**` 与 `modules/runtime-execution/adapters/device/**`
  - 它们是 live consumer 证据，不在本轮处理范围内。
- `tests/contracts/test_bridge_exit_contract.py`
  - 该文件只用于证明仓库仍在跟踪 machine surface cutover，本轮不扩展为仓库级治理。
- `modules/coordinate-alignment/contracts/include/coordinate_alignment/contracts/CoordinateTransformSet.h`
  - 在 contract freeze 前，不应直接改名、删除或迁移，因为它仍是 `process-path` 的 live 输入 seam。

## 11. Next-Phase Input
- migration batches
  - `DispenserModel.*` 迁出 `modules/coordinate-alignment/domain/machine/aggregates/`
  - `CalibrationProcess.*` 与 `CalibrationTypes.h`、`ICalibrationDevicePort.h`、`ICalibrationResultPort.h` 从 M5 owner 面拆离
  - `IHardwareConnectionPort.h` 与 `IHardwareTestPort.h` 从 M5 移出，降到真实 device/runtime/diagnostics owner
- delete batches
  - 若下一阶段确认没有真实 façade，删除 `CoordinateAlignmentApplicationSurface.h` 这类 fake surface
  - 删除或去拓扑化 `services/`、`adapters/`、`examples/` 的空壳实现位
- rename batches
  - 重命名或显式标注 `domain/machine` 为 compat/residual，不再伪装成 M5 canonical owner root
  - 对 README / `module.yaml` / tests README 做单一真值对齐
- contract freeze items
  - 冻结 `CoordinateTransformSet` 是否必须承载数值型 origin offset / rotation / frame 变换参数
  - 冻结 M5 的正式 public seam 是 contracts-only 还是存在真实 application façade
  - 冻结 calibration 与 machine runtime 是否属于 M5，还是仅向 M5 提供输入
- test realignment items
  - 将 CalibrationProcess tests 与 M5 owner gate 脱钩
  - 为 `CoordinateTransformSet` 补数值 contract/golden
  - 为 `coordinate-alignment -> process-path` 补 integration 证明

## 12. Suggested Execution Order
1. 先做 contract freeze：确认 M5 只拥有哪些 alignment facts，以及 `CoordinateTransformSet` 的最小正式字段。
   最小 acceptance criteria：一份单一真值文档能解释 `CoordinateTransformSet`、producer、consumer、allowed dependencies、非目标。
2. 再收缩 public/build surface：让 `siligen_module_coordinate_alignment` 不再公开 `domain/machine` concrete，application surface 要么成真，要么退场。
   最小 acceptance criteria：模块根 target 的对外 surface 不再要求 consumer 包含 `domain/machine/**`。
3. 再迁出 machine/calibration/runtime compat：从 M5 中移走 `DispenserModel`、`CalibrationProcess`、`IHardwareConnectionPort`、`IHardwareTestPort`。
   最小 acceptance criteria：`modules/coordinate-alignment/domain/machine/**` 不再承载 machine/runtime/hardware-test concrete，或被明确降级为 compat 且不再是 M5 live owner。
4. 再重排测试：把 CalibrationProcess 测试迁到真实 owner，把 M5 测试聚焦到 contract/golden/integration。
   最小 acceptance criteria：`modules/coordinate-alignment/tests` 的主证据直接对应 `CoordinateTransformSet` 和 `M5 -> M6` handoff。
5. 最后清理文档与壳层目录：统一 README / `module.yaml` / tests README，移除 fake surface 与空壳 implementation roots。
   最小 acceptance criteria：文档、target topology、物理目录、测试 gate 指向同一组 owner 事实。
