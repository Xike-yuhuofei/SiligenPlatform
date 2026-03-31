# 架构评审文档复核报告

- 复核时间：`2026-03-31 08:21:02 +08:00`
- 复核范围：
  - `docs/process-model/reviews/hmi-application-module-architecture-review-20260331-074433.md`
  - `docs/process-model/reviews/refactor-workflow-ARCH-202-preview-compat-drain-workflow-module-architecture-review-20260331-074657.md`
  - `docs/process-model/reviews/coordinate-alignment-module-architecture-review-20260331-074844.md`
  - `docs/process-model/reviews/dispense-packaging-module-architecture-review-20260331-074840.md`
  - `docs/process-model/reviews/process-path-module-architecture-review-20260331-074844.md`
  - `docs/process-model/reviews/topology-feature-module-architecture-review-20260331-075200.md`
  - `docs/process-model/reviews/process-planning-module-architecture-review-20260331-075201.md`
  - `docs/process-model/reviews/motion-planning-module-architecture-review-20260331-075136-944.md`
  - `docs/process-model/reviews/runtime-execution-module-architecture-review-20260331-075228.md`
- 方法：静态复核文档正文、证据索引、被引用源码与 CMake；未运行构建或测试。

## 总结

- 9 份文档里，发现 `1` 个实质性事实问题，集中在 `dispense-packaging` 评审。
- 另外发现 `4` 个轻度可追溯性问题，主要是“报告声称做过哈希/差异比对，但未把哈希值、diff 摘录或命令保留到文档里”。
- 其余 `4` 份文档，在本次复核范围内未发现明显事实错误，整体可继续作为后续整改输入使用。

## 逐文件结论

| 文档 | 复核结论 | 发现的问题 | 置信度 |
|---|---|---:|---:|
| `hmi-application-module-architecture-review-20260331-074433.md` | 未发现实质性问题 | 0 | 82/100 |
| `refactor-workflow-ARCH-202-preview-compat-drain-workflow-module-architecture-review-20260331-074657.md` | 未发现实质性问题 | 0 | 80/100 |
| `coordinate-alignment-module-architecture-review-20260331-074844.md` | 有轻度可追溯性问题 | 1 | 86/100 |
| `dispense-packaging-module-architecture-review-20260331-074840.md` | 有 1 个实质性问题，另有 1 个轻度可追溯性问题 | 2 | 93/100 |
| `process-path-module-architecture-review-20260331-074844.md` | 未发现实质性问题 | 0 | 84/100 |
| `topology-feature-module-architecture-review-20260331-075200.md` | 有轻度可追溯性问题 | 1 | 84/100 |
| `process-planning-module-architecture-review-20260331-075201.md` | 有轻度可追溯性问题 | 1 | 88/100 |
| `motion-planning-module-architecture-review-20260331-075136-944.md` | 未发现实质性问题 | 0 | 83/100 |
| `runtime-execution-module-architecture-review-20260331-075228.md` | 未发现实质性问题 | 0 | 85/100 |

## 发现的问题

### 1. `dispense-packaging` 评审把 `M9` 的直接依赖错误归因到 `M8` concrete

- 位置：
  - `docs/process-model/reviews/dispense-packaging-module-architecture-review-20260331-074840.md:85-86`
  - `docs/process-model/reviews/dispense-packaging-module-architecture-review-20260331-074840.md:123-130`
  - `docs/process-model/reviews/dispense-packaging-module-architecture-review-20260331-074840.md:207-208`
- 文档结论：
  - 该评审写成了“`runtime-execution` 实现里直接构造 `M8` concrete `DispensingProcessService`”。
- 复核结果：
  - 现有证据只能支持“`runtime-execution` 直接依赖 legacy `domain/dispensing/domain-services/DispensingProcessService.h`，且这条 include 解析链来自 `workflow` 旧聚合上下文”，不能支持“它已经直接绑定到 `M8` concrete”。
- 证据：
  - `modules/runtime-execution/application/usecases/dispensing/DispensingExecutionUseCase.cpp:5` 只写了相对旧域头：`domain/dispensing/domain-services/DispensingProcessService.h`
  - `modules/runtime-execution/application/CMakeLists.txt:45-82` 的私有 include 只来自 `PROCESS_RUNTIME_CORE_*` 与本模块目录，公开 link 也没有 `siligen_dispense_packaging_domain_dispensing`
  - `modules/workflow/CMakeLists.txt:53-70` 说明 `PROCESS_RUNTIME_CORE_*` 会把 `workflow/domain/include` 与 workflow 兼容上下文注入进来
  - 反过来，`modules/dispense-packaging/domain/dispensing/CMakeLists.txt:16-26` 虽然编译了 `DispensingProcessService.cpp`，但它并未被 `runtime-execution/application` 直接链接
- 影响：
  - 这会把问题定性从“`M9` 仍绑在 legacy workflow execution service”误写成“`M9` 已直接绑到 `M8` live service”。
  - 迁移修复方向会因此跑偏。
- 判断：
  - 这是实质性事实错误，不是措辞问题。
- 置信度：`93/100`

### 2. `coordinate-alignment` 评审的哈希结论可复现性不足

- 位置：
  - `docs/process-model/reviews/coordinate-alignment-module-architecture-review-20260331-074844.md:22`
  - `docs/process-model/reviews/coordinate-alignment-module-architecture-review-20260331-074844.md:80`
- 文档结论：
  - 声称本地 SHA256 比对表明 `DispenserModel.cpp` 与 `CalibrationProcess.cpp` 两对文件完全一致。
- 复核结果：
  - 这个判断本身成立，但文档没有保留哈希值、比对命令或最小 diff 摘录。
- 影响：
  - 审查报告可读，但证据链不可直接复现，后续复核必须重新做一次文件比对。
- 判断：
  - 这是轻度可追溯性问题，不影响主结论方向。
- 置信度：`90/100`

### 3. `dispense-packaging` 评审的重复实现结论同样缺少可复现比对输出

- 位置：
  - `docs/process-model/reviews/dispense-packaging-module-architecture-review-20260331-074840.md:101`
- 文档结论：
  - 声称 `TriggerPlanner.cpp`、`DispensingController.cpp`、`TriggerPlan.h`、`DispensingPlan.h` 本地哈希一致，`DispensingProcessService.cpp` 已分叉。
- 复核结果：
  - 重复文件存在，方向判断基本可信；但报告里没有保留任何哈希值、diff 摘录或比对命令。
- 影响：
  - 这会降低该结论的可审计性。
- 判断：
  - 轻度可追溯性问题。
- 置信度：`88/100`

### 4. `topology-feature` 评审的 shadow copy 判定缺少哈希落档

- 位置：
  - `docs/process-model/reviews/topology-feature-module-architecture-review-20260331-075200.md:34`
- 文档结论：
  - 声称 `ContourAugmenterAdapter.cpp/.stub.cpp` 在 `topology-feature` 与 `workflow` 下 SHA256 完全一致。
- 复核结果：
  - 结论成立，但报告未保存哈希值或比对命令。
- 影响：
  - 会让“shadow copy 不是纯头转发”这个关键点缺少直接可复核证据。
- 判断：
  - 轻度可追溯性问题。
- 置信度：`90/100`

### 5. `process-planning` 评审存在轻微证据精度不足

- 位置：
  - `docs/process-model/reviews/process-planning-module-architecture-review-20260331-075201.md:112`
  - `docs/process-model/reviews/process-planning-module-architecture-review-20260331-075201.md:369`
  - `docs/process-model/reviews/process-planning-module-architecture-review-20260331-075201.md:372`
- 文档结论：
  - 使用了 `modules/process-planning/domain/configuration/*.cpp`、`ConfigFileAdapter.*` 这类通配写法作为证据项。
- 复核结果：
  - 结论方向基本正确，但这类索引不够精确，不利于逐条审阅。
- 影响：
  - 会降低证据索引的检索效率与可复盘性。
- 判断：
  - 轻度可追溯性问题。
- 置信度：`88/100`

## 未发现实质性问题的文档

- `docs/process-model/reviews/hmi-application-module-architecture-review-20260331-074433.md`
  - 关键结论与仓库现状一致，例如：
    - `apps/planner-cli/CMakeLists.txt:26` 确实链接 `siligen_module_hmi_application`
    - `modules/hmi-application/application/hmi_application/startup.py:10,22-24` 与 `preview_session.py:7,18-19` 确实直接依赖 Qt 和 app concrete client
    - `modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md:367-381` 确实仍记录不存在的 `src/CommandHandlers.cpp`

- `docs/process-model/reviews/refactor-workflow-ARCH-202-preview-compat-drain-workflow-module-architecture-review-20260331-074657.md`
  - 关键结论与当前 `workflow` CMake 和 public include 暴露方式一致。

- `docs/process-model/reviews/process-path-module-architecture-review-20260331-074844.md`
  - `alignment` 字段存在但 live facade 未消费、`contracts` 直接 alias `domain`、`workflow` 保留 trajectory 副本，这几条都能被源码直接支持。

- `docs/process-model/reviews/motion-planning-module-architecture-review-20260331-075136-944.md`
  - `IInterpolationPort` 仍留在 `M7`、`MotionBufferController.h` 直接桥接 `workflow`、contracts 直接 alias domain，这些判断成立。

- `docs/process-model/reviews/runtime-execution-module-architecture-review-20260331-075228.md`
  - `M9 -> M0` 双向耦合、runtime contracts 兼容壳化、device contract 重复树、public 头直连私有 `src` 的判断都能被当前代码支撑。

## 建议

- 需要立即回改：`dispense-packaging-module-architecture-review-20260331-074840.md`
  - 把“`M9` 直接构造 `M8 concrete`”改成“`M9` 仍直接依赖 legacy workflow/domain execution service，且该服务与 M8 现有实现形成重复/漂移风险”。

- 建议顺手补强：
  - `coordinate-alignment`
  - `dispense-packaging`
  - `topology-feature`
  - `process-planning`
  - 这 4 份报告都应把哈希值、`git diff --no-index` 摘录或复核命令附到正文或附录里，避免关键判断只能靠口述。
