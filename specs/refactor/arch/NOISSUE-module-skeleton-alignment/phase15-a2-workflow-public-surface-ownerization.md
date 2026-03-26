# Phase 15 A2: workflow public surface ownerization

更新时间：`2026-03-25`

## 目标

在不修改 `runtime-execution` 与外部消费者的前提下，继续收敛 `workflow` 的 public surface，使其不再回指 `process-runtime-core` 或其他 bridge payload，并尽量把 public owner 事实落到真正的 canonical 面。

## 本次 owner 化结果

### 1. 已完成 owner 化的 public header

- `modules/workflow/domain/include/domain/planning/ports/ISpatialIndexPort.h`
  - 不再是对 `domain/domain/planning/ports/ISpatialIndexPort.h` 的薄 wrapper。
  - 该 public header 现已承载正式 owner 定义。
- `modules/workflow/domain/domain/planning/ports/ISpatialIndexPort.h`
  - 已降级为 legacy bridge header，只回指 canonical public header。

### 2. public include roots 当前口径

- `modules/workflow/domain/include`
- `modules/workflow/application/include`
- `modules/workflow/adapters/include`

以上 public roots 当前已不包含任何回指 `process-runtime-core/src` 或 `src/legacy` 的 header。

## 仍保留的 wrapper-backed public headers

以下 `workflow/domain/include` headers 仍保留为 owner-forwarding wrapper，目标 owner 为 `modules/motion-planning/domain/motion`：

- `domain/motion/value-objects/MotionTrajectory.h`
- `domain/motion/value-objects/MotionTypes.h`
- `domain/motion/value-objects/TrajectoryAnalysisTypes.h`
- `domain/motion/value-objects/TrajectoryTypes.h`
- `domain/motion/ports/IInterpolationPort.h`
- `domain/motion/ports/IVelocityProfilePort.h`
- `domain/motion/domain-services/VelocityProfileService.h`
- `domain/motion/domain-services/SevenSegmentSCurveProfile.h`
- `domain/motion/domain-services/interpolation/InterpolationCommandValidator.h`
- `domain/motion/domain-services/interpolation/TrajectoryInterpolatorBase.h`
- `domain/motion/domain-services/interpolation/ValidatedInterpolationPort.h`

## 为什么这 11 个 wrapper 还不能移除

- `process-path/domain/trajectory` 当前 public include 仍直接消费 `SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR`，没有显式携带 `motion-planning` public root。
- `runtime-execution/runtime/host` 与若干 workflow/application 头当前也继续通过 `domain/motion/...` 兼容路径消费这些类型。
- 直接删除这些 wrapper 会导致：
  - `process-path` 的 `MotionPlanner.h` 无法解析 `domain/motion/value-objects/MotionTrajectory.h`
  - `workflow/domain/include/domain/motion/value-objects/HardwareTestTypes.h` 等仍以同目录 sibling include 方式引用 `MotionTypes.h` 的头失效
- 因此，这 11 个头目前属于“兼容性必需的 owner-forwarding wrapper”，而不是回指 bridge payload 的遗留问题。

## 内部 bridge 收敛

以下 legacy bridge headers 已从“回指 workflow public wrapper”改为“回指真正 owner-backed canonical public surface”：

- `modules/workflow/domain/domain/motion/value-objects/`
  - `MotionTrajectory.h`
  - `MotionTypes.h`
  - `TrajectoryAnalysisTypes.h`
  - `TrajectoryTypes.h`
- `modules/workflow/domain/domain/motion/ports/`
  - `IInterpolationPort.h`
  - `IVelocityProfilePort.h`
- `modules/workflow/domain/domain/motion/domain-services/`
  - `VelocityProfileService.h`
  - `SevenSegmentSCurveProfile.h`
- `modules/workflow/domain/domain/motion/domain-services/interpolation/`
  - `InterpolationCommandValidator.h`
  - `TrajectoryInterpolatorBase.h`
  - `ValidatedInterpolationPort.h`

这些 bridge header 当前都通过 canonical include 名称转发到真正 owner 面，不再依赖 workflow public wrapper 的物理文件实现。

## 验证结果

- 通过：`cmake -S D:\Projects\SS-dispense-align -B C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build`
- 通过：`cmake --build C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build --config Debug --target siligen_unit_tests process_runtime_core_motion_runtime_assembly_test process_runtime_core_pb_path_source_adapter_test siligen_runtime_host_unit_tests`
- 通过：`ctest --test-dir C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build -C Debug -R "process_runtime_core_motion_runtime_assembly_test|process_runtime_core_pb_path_source_adapter_test|siligen_runtime_host_unit_tests" --output-on-failure`
- 通过：`rg -n "process-runtime-core/src|src/legacy" D:\Projects\SS-dispense-align\modules\workflow\domain\include D:\Projects\SS-dispense-align\modules\workflow\application\include D:\Projects\SS-dispense-align\modules\workflow\adapters\include`
  - 无匹配

## 结论

- `workflow` 的 public surface 已退出对 `process-runtime-core` / `src/legacy` 的回指。
- `ISpatialIndexPort.h` 已完成真正 public owner 化。
- `motion-planning` 相关 public headers 仍残留 11 个兼容 wrapper，但这些 wrapper 当前全部回指 canonical owner，而不是 bridge payload。
- 若后续要继续清空这 11 个 wrapper，必须先让 `process-path` 与其他消费者显式携带 `motion-planning` public root，而不是只依赖 `workflow/domain/include`。
