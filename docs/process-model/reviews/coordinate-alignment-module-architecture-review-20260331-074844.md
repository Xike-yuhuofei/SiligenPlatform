# coordinate-alignment 模块架构审查

- 审查时间：2026-03-31 07:48:44
- 审查范围：`modules/coordinate-alignment/`，以及判断边界所需的少量关联文件
- 审查目标：职责、边界、依赖、落点、构建组织、结构性问题

## 1. 模块定位

- 核心职责：按声明，`M5 coordinate-alignment` 应负责 `S6-S7` 的静态坐标链解析与对位补偿收敛，即把 `ProcessPlan` 转成 `CoordinateTransformSet` / `AlignmentCompensation`，供 `M6 process-path` 消费。
- 业务位置：位于 `M4 process-planning` 与 `M6 process-path` 之间；`M6` 的请求对象直接持有 `CoordinateTransformSet`。
- 主要输入：声明上应是 `ProcessPlan`、治具/设备基准、标定与测量数据；代码里实际输入却是 `CalibrationRequest`、`ICalibrationDevicePort`、`ICalibrationResultPort`、`DispensingTask`、运动/连接配置。
- 主要输出：声明上应是 `CoordinateTransformSet` / `AlignmentCompensation`；代码里实际输出是极简 `CoordinateTransformSet` 占位结构、`CalibrationResult`、设备状态与任务队列状态。
- 应当依赖谁：按模块白名单，应依赖 `process-planning/contracts` 与 `shared`；若存在动态对位执行，按现有仓库说明也应通过 `runtime_execution` 契约交互，而不是直接持有 machine/runtime 兼容端口。该后半句属于基于仓库说明的推断。
- 谁应当依赖它：`process-path` 及上层编排入口应依赖其契约或应用面；当前代码里明确可见的正式 consumer 只有 `process-path` 对 `CoordinateTransformSet` 契约的消费，真实 M5 应用层 consumer 证据不足。

## 2. 声明边界 vs 实际实现

- 文档声明：M5 owner 是 `CoordinateTransformSet` 和 `AlignmentCompensation`，输入契约包括 `ResolveCoordinateTransforms` / `RunAlignment` / `MarkAlignmentNotRequired`，输出契约包括 `CoordinateTransformResolved` / `AlignmentSucceeded` / `AlignmentFailed` 等。
- 实际代码：模块内真正可编译实现只有 `DispenserModel.cpp` 与 `CalibrationProcess.cpp`；`application` 只有一个空壳头文件；`services/adapters/examples` 只有 README；`tests` 只有 `.gitkeep`。
- 一致性判断：不一致，而且是根本性偏差。`CoordinateTransformSet` 契约只剩 `alignment_id/plan_id/valid/transforms/owner_module`，与规格要求的变换链字段、结果字段、基准字段不一致。
- 偏差位置：该模块应有的 `AlignmentCompensation`、S6/S7 命令/事件、真实应用入口未落在本模块；在 `modules apps shared tests` 范围内搜索这些标识符无代码命中，当前证据更支持“未实现”，而不是“实现落在别处”。
- 重复/越界：`domain/machine` 把设备状态、任务队列、连接、心跳、点动/回零/CMP/插补测试拉进了 M5；同时 `workflow/domain/domain/machine` 又保留同名同职责副本，本地哈希比对显示 `DispenserModel.cpp` 与 `CalibrationProcess.cpp` 两对文件 SHA256 完全一致，具体值见附录。

## 3. 模块内部结构审查

- 目录层次：表面完整，实质空心。`contracts/` 只有一个头文件，`application/` 只有一个 include 壳，`domain/machine/` 承担全部真实代码，`services/adapters/tests/examples` 没有实现。
- 公共接口与内部实现：形式上分离，实质未分离。`application_public` 只是把 `contracts` 与 `domain_machine` 重新导出，没有独立用例面。
- 混杂层次：`IHardwareConnectionPort`、`IHardwareTestPort` 把 motion/diagnostics/hardware-test/runtime compatibility 都塞进 `domain/machine/ports`，属于领域逻辑、兼容桥、硬件能力混放。
- 明确判断：内部结构 `混乱`。原因不是目录不整齐，而是 owner 骨架与真实职责错位，构建入口又把错位内容当成模块核心导出。

## 4. 对外依赖与被依赖关系

### 合理依赖

- `shared` 类型/工具依赖合理。
- `process-path` 仅通过 `CoordinateTransformSet` 契约消费 M5，这符合“下游吃契约”的方向。

### 可疑依赖

- `application_public` 没有独立行为，只重导 `domain_machine`；这意味着外部一旦依赖模块 public surface，拿到的仍是 machine 兼容实现而非坐标对齐用例。

### 高风险依赖

- `module.yaml` 白名单不含 `workflow`，但备注与 `domain/machine/CMakeLists.txt` 明确消费 workflow public headers。
- `IHardwareTestPort` 直接依赖 `domain/motion` 和 `domain/diagnostics` 类型。
- `runtime-execution` 适配器继续实现这些 legacy 端口。
- `runtime-execution` 仍以 `DispenserModel` 做 machine execution 兼容桥。

### 工程后果

- M5 无法独立演进，owner 边界与 include graph 脱节。
- 显式 CMake 环依赖证据不足，但存在明显语义环与迁移停滞风险。

## 5. 结构性问题清单

### 1. M5 owner 对象空心化

- 现象：文档要求 `CoordinateTransformSet + AlignmentCompensation + S6/S7 命令/事件`，代码只有极简 `CoordinateTransformSet` 和空壳 application surface。
- 涉及文件：
  - `docs/architecture/modules/coordinate-alignment.md`
  - `modules/coordinate-alignment/contracts/include/coordinate_alignment/contracts/CoordinateTransformSet.h`
  - `modules/coordinate-alignment/application/include/coordinate_alignment/application/CoordinateAlignmentApplicationSurface.h`
- 为什么这是结构问题而不是局部实现问题：缺的不是某个算法，而是模块 owner artifact、命令面、结果面整体未建立。
- 可能后果：M6 只能依赖占位结构，S6/S7 无法形成稳定版本链。
- 优先级：P0

### 2. `coordinate-alignment` 承载了 machine/runtime 兼容子域

- 现象：`domain/machine` 明确负责设备状态、任务编排、硬件连接、统计、标定；`DispenserModel` 维护点胶任务队列与运行状态；`IHardwareTestPort` 覆盖点动/回零/CMP/插补/诊断/急停。
- 涉及文件：
  - `modules/coordinate-alignment/domain/machine/README.md`
  - `modules/coordinate-alignment/domain/machine/aggregates/DispenserModel.h`
  - `modules/coordinate-alignment/domain/machine/ports/IHardwareTestPort.h`
- 为什么这是结构问题而不是局部实现问题：这是 bounded context 错放，不是类名不准。
- 可能后果：规划链模块被运行态变化牵动，长期无法稳定。
- 优先级：P0

### 3. `workflow` 与 M5 双 owner 重复实现

- 现象：`workflow/domain/domain/README.md` 仍把 `CalibrationProcess` / `DispenserModel` 定义为唯一规则来源，且 `workflow/domain/domain/machine/CMakeLists.txt` 同样构建这两个源文件；本地哈希比对显示两处 `.cpp` 完全一致，具体 SHA256 值见附录。
- 涉及文件：
  - `modules/workflow/domain/domain/README.md`
  - `modules/workflow/domain/domain/machine/CMakeLists.txt`
  - `modules/coordinate-alignment/domain/machine/CMakeLists.txt`
- 为什么这是结构问题而不是局部实现问题：同一业务事实存在双 authoritative source。
- 可能后果：后续任何修复都要双改，消费者会继续绕过 M5。
- 优先级：P0

### 4. 依赖方向违背声明

- 现象：`module.yaml` 白名单只有 `process-planning/contracts` 和 `shared`，但备注承认消费 workflow headers，`domain/machine` 也直接引入 `SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR`，根 CMake 还全局注入该 include path。
- 涉及文件：
  - `modules/coordinate-alignment/module.yaml`
  - `modules/coordinate-alignment/domain/machine/CMakeLists.txt`
  - `CMakeLists.txt`
- 为什么这是结构问题而不是局部实现问题：target graph 与模块边界不一致。
- 可能后果：M5 无法被独立测试、替换或下沉为真正 planning 模块。
- 优先级：P1

## 6. 模块结论

- 宣称职责：静态坐标链解析、对位补偿、无补偿显式化、超限判定，owner 为 `CoordinateTransformSet` / `AlignmentCompensation`。
- 实际职责：legacy `machine/calibration` 兼容副本，加一个极简 `CoordinateTransformSet` 占位契约。
- 是否职责单一：否。
- 是否边界清晰：否。
- 是否被侵入：是，但主要表现为 `workflow/runtime` 旧 machine 语义仍与该目录混叠；对真正的坐标变换 owner，当前更像“缺位”，不是“被正式迁走”，是否别处已有正式实现证据不足。
- 是否侵入别人：是，明显侵入 `workflow` 的 machine 子域以及 `runtime-execution/motion/diagnostics` 的兼容与硬件测试职责。
- 是否适合作为稳定业务模块继续演进：不适合，至少在当前形态下不适合。
- 最终评级：`高风险`

## 7. 修复顺序

### 1. 先收口 M5 的 owner 契约与边界

- 目标：把 `CoordinateTransformSet/AlignmentCompensation/S6-S7 commands-events` 定成唯一正式面，并同步修正文档与 `module.yaml`。
- 涉及目录/文件：
  - `modules/coordinate-alignment/contracts/`
  - `modules/coordinate-alignment/application/`
  - `modules/coordinate-alignment/README.md`
  - `modules/coordinate-alignment/module.yaml`
  - `docs/architecture/modules/coordinate-alignment.md`
- 收益：先把“它到底是什么”固定下来。
- 风险：`process-path` 现有占位契约会被迫调整。

### 2. 再迁移 `domain/machine/*` 与 legacy ports

- 目标：把 `DispenserModel`、`CalibrationProcess`、`IHardwareConnectionPort`、`IHardwareTestPort` 从 M5 拆回 `workflow/runtime-execution` 或独立 machine/runtime owner，消除双副本。
- 涉及目录/文件：
  - `modules/coordinate-alignment/domain/machine/`
  - `modules/workflow/domain/domain/machine/`
  - `modules/workflow/domain/include/domain/machine/`
  - `modules/runtime-execution/...`
- 收益：去掉最主要的职责错位与重复实现。
- 风险：include 路径和兼容桥需要过渡期。

### 3. 最后建立真实的 M5 应用/领域实现

- 目标：新增真正的 transform/alignment 子域与 use-case surface，让 `process-path` 只依赖 M5 契约或应用面，不再看到 machine 兼容实现。
- 涉及目录/文件：
  - `modules/coordinate-alignment/application/`
  - `modules/coordinate-alignment/contracts/`
  - 建议新增 `modules/coordinate-alignment/domain/transform` 或 `modules/coordinate-alignment/domain/alignment`
  - `modules/process-path/...` consumer
- 收益：M5 才能成为稳定 planning 模块。
- 风险：会牵动 M6 请求结构和现有测试基线。

## 8. 证据索引

| 文件路径 | 你从该文件得出的判断 | 该判断支撑了哪条结论 |
|---|---|---|
| `docs/architecture/modules/coordinate-alignment.md` | M5 被正式定义为 S6-S7 坐标链与对位补偿 owner | 1, 2, 6 |
| `modules/coordinate-alignment/README.md` | 模块 README 宣称面向 M6 提供对齐结果，但承认唯一真实实现承载面是 `domain/machine` | 1, 2, 3, 6 |
| `modules/coordinate-alignment/module.yaml` | 依赖白名单不含 `workflow`，却备注消费 workflow headers | 4, 5, 6 |
| `modules/coordinate-alignment/contracts/include/coordinate_alignment/contracts/CoordinateTransformSet.h` | owner artifact 只是占位 struct，未表达规格要求的变换链语义 | 1, 2, 5, 6 |
| `modules/coordinate-alignment/application/include/coordinate_alignment/application/CoordinateAlignmentApplicationSurface.h` | 应用层 public surface 为空壳 | 2, 3, 5 |
| `modules/coordinate-alignment/application/CMakeLists.txt` | 应用层只是重导 `contracts` 与 `domain_machine`，没有独立用例实现 | 2, 3, 5 |
| `modules/coordinate-alignment/domain/machine/README.md` | 模块核心实现实际是设备状态机、任务队列、硬件连接与标定 | 2, 3, 5, 6 |
| `modules/coordinate-alignment/domain/machine/aggregates/DispenserModel.h` | 实际建模对象是点胶机运行状态与任务管理，不是坐标对齐结果 | 2, 3, 5, 6 |
| `modules/coordinate-alignment/domain/machine/ports/IHardwareConnectionPort.h` | M5 端口直接耦合连接、心跳等运行态职责 | 3, 4, 5, 6 |
| `modules/coordinate-alignment/domain/machine/ports/IHardwareTestPort.h` | M5 端口直接耦合点动、回零、CMP、插补、诊断、安全等跨域职责 | 3, 4, 5, 6 |
| `modules/workflow/domain/domain/README.md` | `workflow` 仍把 `CalibrationProcess` / `DispenserModel` 当作唯一规则来源 | 2, 4, 5, 6 |
| `modules/workflow/domain/domain/machine/CMakeLists.txt` | `workflow` 仍继续构建这两个 machine 源文件 | 2, 4, 5, 6 |
| `modules/coordinate-alignment/domain/machine/CMakeLists.txt` | M5 根实现 target 直接建立在 `domain/machine` 之上，且消费 workflow include | 3, 4, 5, 6 |
| `CMakeLists.txt` | 根构建系统全局注入 workflow domain include path | 4, 5, 6 |
| `modules/runtime-execution/runtime/host/runtime/system/LegacyMachineExecutionStateAdapter.cpp` | runtime 侧仍围绕 `DispenserModel` 做 machine execution 兼容桥 | 4, 5, 6 |
| `modules/runtime-execution/adapters/device/src/adapters/motion/controller/connection/HardwareConnectionAdapter.h` | `runtime-execution` 继续实现 M5 旧连接端口 | 4, 5, 6 |
| `modules/runtime-execution/adapters/device/src/adapters/diagnostics/health/testing/HardwareTestAdapter.h` | `runtime-execution` 继续实现 M5 旧硬件测试端口 | 4, 5, 6 |
| `modules/process-path/contracts/include/process_path/contracts/PathGenerationRequest.h` | 下游实际只消费 `CoordinateTransformSet` 契约 | 1, 2, 4, 6 |
| `modules/process-path/tests/unit/application/services/process_path/ProcessPathFacadeTest.cpp` | `process-path` 测试只验证 M5 契约占位字段，而非真实坐标链语义 | 1, 2, 4, 6 |

## 附加说明

- 对 `AlignmentCompensation`、`ResolveCoordinateTransforms`、`RunAlignment`、`MarkAlignmentNotRequired`、`CoordinateTransformResolved`、`AlignmentSucceeded`、`AlignmentFailed`、`CompensationOutOfRange` 的代码搜索在 `modules apps shared tests` 范围内未命中实现，当前证据更支持“未落地”，不是“落在别处”。
- 显式构建环依赖证据不足；当前可以明确判断的是语义环、兼容桥滞留和 owner 漂移。

## 复核附录

- 2026-03-31 二次复核补充了重复文件的 SHA256 证据，使用命令：
  - `Get-FileHash <path> -Algorithm SHA256`
- 哈希结果：
  - `modules/coordinate-alignment/domain/machine/aggregates/DispenserModel.cpp`
    - `2F8E4CB528CAFED999536B91731003843E75B60F2DF88E5D53877E01E80A57EC`
  - `modules/workflow/domain/domain/machine/aggregates/DispenserModel.cpp`
    - `2F8E4CB528CAFED999536B91731003843E75B60F2DF88E5D53877E01E80A57EC`
  - `modules/coordinate-alignment/domain/machine/domain-services/CalibrationProcess.cpp`
    - `58498125799319552FB17BF995BE02FD21FD98C3B7213FF9457924AE59FE8237`
  - `modules/workflow/domain/domain/machine/domain-services/CalibrationProcess.cpp`
    - `58498125799319552FB17BF995BE02FD21FD98C3B7213FF9457924AE59FE8237`
