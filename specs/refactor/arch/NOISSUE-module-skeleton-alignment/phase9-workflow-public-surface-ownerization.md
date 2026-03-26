# Phase 9: workflow canonical public surface ownerization

更新时间：`2026-03-25`

## 背景

`Phase 8` 之后，`runtime-execution` 已进入 canonical-first build 状态，并已退出对 `workflow/process-runtime-core/src` 的直接显式 include 依赖。

剩余的主阻塞不再是 `runtime-execution` 自身 build root 切换，而是 `workflow` 对外 public surface 仍有 wrapper-backed 残留，导致下游消费者在只拿 canonical public include roots 时，无法稳定解析全部 public headers。

本阶段目标是：

1. 把 `workflow` 的 public surface 从 wrapper-backed 提升到 owner-backed。
2. 保持 `process-runtime-core` 仍为内部 bridge-backed implementation，不误宣称 hard closeout。
3. 用真实下游 target 构建确认 `planner-cli`、`transport-gateway`、`runtime-host` 等消费者可通过 canonical public surface 闭环编译。

## 实施内容

### 1. canonical layer CMake 继续作为正式 public 入口

以下 canonical layer 继续作为 `workflow` 的正式 public 入口：

- `modules/workflow/domain/CMakeLists.txt`
- `modules/workflow/application/CMakeLists.txt`
- `modules/workflow/adapters/CMakeLists.txt`
- `modules/workflow/CMakeLists.txt`

主模块 CMake 继续保留：

- `siligen_workflow_domain_public`
- `siligen_workflow_application_public`
- `siligen_workflow_adapters_public`
- `siligen_workflow_runtime_consumer_public`

旧 `process-runtime-core` target 仍通过 `$<LINK_ONLY:...>` 聚合，避免 `src` include 重新透传为 public surface。

### 2. 补齐遗漏的 owner-backed public dependency headers

为消除 canonical public headers 对旧 `src` 头的隐式依赖，本阶段补齐了以下 owner-backed headers：

- `modules/workflow/domain/include/domain/motion/domain-services/interpolation/InterpolationCommandValidator.h`
- `modules/workflow/domain/include/domain/diagnostics/aggregates/TestRecord.h`
- `modules/workflow/domain/include/domain/diagnostics/value-objects/TestDataTypes.h`
- `modules/workflow/domain/include/domain/motion/value-objects/TrajectoryAnalysisTypes.h`

同时，将以下 legacy headers 降级为 bridge headers，统一回指 canonical owner headers：

- `modules/workflow/process-runtime-core/src/domain/motion/domain-services/interpolation/InterpolationCommandValidator.h`
- `modules/workflow/process-runtime-core/src/domain/diagnostics/aggregates/TestRecord.h`
- `modules/workflow/process-runtime-core/src/domain/diagnostics/value-objects/TestDataTypes.h`
- `modules/workflow/process-runtime-core/src/domain/motion/value-objects/TrajectoryAnalysisTypes.h`

### 3. 收紧 public include 可见性，但保留非 src 公共依赖

`modules/workflow/domain/CMakeLists.txt` 已调整为：

- 继续公开 `modules/workflow/domain/include`
- 同时公开 `process-runtime-core/modules/process-core/include`
- 同时公开 `process-runtime-core/modules/motion-core/include`
- 不公开 `process-runtime-core/src`

这样可以满足 `MotionCoreInterlockBridge.h` 对 `motion-core` 正式公共头的依赖，同时保持“canonical public surface 不回退到 `src` root”的约束。

### 4. runtime-execution 消费链验证

本阶段继续保留 `runtime-execution` 的以下事实：

- `contracts/device`、`adapters/device`、`runtime/host` 为 canonical build roots
- `device-contracts`、`device-adapters`、`runtime-host` 保留为 bridge forwarder
- `runtime-host/tests` 与 `device-adapters/vendor/multicard` 继续锁定为 strict shell bridge roots

在此基础上，重新验证了外部消费者 target 对 `workflow` canonical public surface 的真实消费链。

## 验证

以下验证已通过：

1. `python scripts/migration/validate_workspace_layout.py --wave "Wave 9"`
2. `.\build.ps1 -Profile Local -Suite contracts`
3. `ctest --test-dir C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build -C Debug -R "siligen_runtime_host_unit_tests|siligen_unit_tests|siligen_pr1_tests" --output-on-failure`
4. `cmake --build C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build --config Debug --target siligen_planner_cli siligen_transport_gateway`

补充说明：

- 第 4 项首次串行重跑时暴露了真实问题，不是文件锁：`ValidatedInterpolationPort.h` 与 `ITestRecordRepository.h` 的依赖头尚未全部 owner 化。
- 完成 owner header 补齐与 public include roots 修正后，同一构建命令已成功通过。

## 关闭口径

`Phase 9` 完成后，当前正确口径为：

- `workflow` canonical public headers 已 owner-backed
- `workflow` canonical public CMake layers 已成为正式 public 入口
- `process-runtime-core` 仍为内部 bridge-backed implementation
- `runtime-execution` 已通过 canonical public surface 消费链验证

本阶段仍然不是 hard closeout。以下事项继续保留到后续 bridge drain：

- `modules/workflow/process-runtime-core`
- `modules/runtime-execution/runtime-host`
- `modules/runtime-execution/device-adapters`
- `modules/runtime-execution/device-contracts`

因此，本阶段只能宣告：

`workflow` 与 `runtime-execution` 已达到更强的 bridge-backed closeout，不得宣称“历史 bridge 已物理清空”。
