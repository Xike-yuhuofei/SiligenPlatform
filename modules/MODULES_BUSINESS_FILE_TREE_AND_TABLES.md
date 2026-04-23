# modules 业务文件树与实现文件说明

本文档按模块输出两部分内容：

- 文件树：展示模块内代码结构，不附文件说明。
- 表格：按最后一级文件夹展示实现文件在点胶流程中的具体作用，仅纳入 `.cpp`、`.cc`、`.cxx`、`.py`。

- 表格列含义：`流程阶段`、`触发条件`、`当前职责`、`输出结果`、`失败影响`
- 已忽略表格说明：`CMakeLists.txt`、`module.yaml`、所有 `.h/.hpp`、文档、缓存、占位文件
- 文件树统计范围：`1000` 个文件
- 表格说明统计范围：`362` 个实现文件

## 模块概览

- `M5 coordinate-alignment`：owner `CoordinateTransformSet`，文件树 `12` 项，实现文件说明 `2` 项。
- `M8 dispense-packaging`：owner `ExecutionPackage`，文件树 `44` 项，实现文件说明 `14` 项。
- `M2 dxf-geometry`：owner `CanonicalGeometry`，文件树 `57` 项，实现文件说明 `39` 项。
- `M11 hmi-application`：owner `UIViewModel`，文件树 `6` 项，实现文件说明 `2` 项。
- `M1 job-ingest`：owner `JobDefinition`，文件树 `13` 项，实现文件说明 `8` 项。
- `M7 motion-planning`：owner `MotionTrajectory`，文件树 `154` 项，实现文件说明 `54` 项。
- `M6 process-path`：owner `ProcessPath`，文件树 `26` 项，实现文件说明 `6` 项。
- `M4 process-planning`：owner `ProcessPlan`，文件树 `12` 项，实现文件说明 `5` 项。
- `M9 runtime-execution`：owner `ExecutionSession`，文件树 `164` 项，实现文件说明 `79` 项。
- `M3 topology-feature`：owner `TopologyModel`，文件树 `6` 项，实现文件说明 `2` 项。
- `M10 trace-diagnostics`：owner `ExecutionRecord`，文件树 `5` 项，实现文件说明 `1` 项。
- `M0 workflow`：owner `WorkflowRun`，文件树 `500` 项，实现文件说明 `150` 项。

## coordinate-alignment

- 模块编号：`M5`
- Owner 对象：`CoordinateTransformSet`
- 所属分组：`planning`
- 模块职责：规划链中的坐标系基准对齐语义；坐标变换与对齐参数的模块 owner 边界；面向 `M6 process-path` 消费的对齐结果口径

**文件树**
```text
coordinate-alignment/
├── domain/
│   └── machine/
│       ├── aggregates/
│       │   ├── DispenserModel.cpp
│       │   └── DispenserModel.h
│       ├── domain-services/
│       │   ├── CalibrationProcess.cpp
│       │   └── CalibrationProcess.h
│       ├── ports/
│       │   ├── ICalibrationDevicePort.h
│       │   ├── ICalibrationResultPort.h
│       │   ├── IHardwareConnectionPort.h
│       │   └── IHardwareTestPort.h
│       ├── value-objects/
│       │   └── CalibrationTypes.h
│       └── CMakeLists.txt
├── CMakeLists.txt
└── module.yaml
```

**实现文件说明表**

**coordinate-alignment/domain/machine/aggregates/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `DispenserModel.cpp` | 点胶前的设备就绪与任务装载阶段 | 当系统装载点胶任务、切换设备状态、暂停/恢复任务或结束任务时触发。 | 维护设备状态、当前任务、待执行任务和完成统计，判断设备是否允许进入正式点胶。 | 输出设备当前状态、任务状态、任务队列和统计结果，供校准、调度和执行侧判断是否继续。 | 如果这里出错，后续路径坐标和实际出胶位置会整体偏移，严重时应中止点胶。 |

**coordinate-alignment/domain/machine/domain-services/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `CalibrationProcess.cpp` | 点胶前的校准与坐标确认阶段 | 当正式点胶前需要确认设备标定和坐标基准是否可信时触发。 | 推进校准请求的校验、执行、验证和结果收敛，确保后续坐标和落点可信。 | 输出校准状态、校准结果和成功/失败结论，供路径生成和后续落点控制使用。 | 如果这里出错，后续路径坐标和实际出胶位置会整体偏移，严重时应中止点胶。 |

## dispense-packaging

- 模块编号：`M8`
- Owner 对象：`ExecutionPackage`
- 所属分组：`planning`
- 模块职责：承接 `DispenseTimingPlan`、`ExecutionPackage` 的正式 owner 语义。；负责时序构建、执行包组装与离线校验的模块边界。；向 `M9 runtime-execution` 输出可消费但不可回写的执行准备事实。

**文件树**
```text
dispense-packaging/
├── domain/
│   └── dispensing/
│       ├── compensation/
│       ├── domain-services/
│       │   ├── ArcTriggerPointCalculator.cpp
│       │   ├── ArcTriggerPointCalculator.h
│       │   ├── CMPTriggerService.cpp
│       │   ├── CMPTriggerService.h
│       │   ├── DispenseCompensationService.cpp
│       │   ├── DispenseCompensationService.h
│       │   ├── DispensingController.cpp
│       │   ├── DispensingController.h
│       │   ├── PathOptimizationStrategy.cpp
│       │   ├── PathOptimizationStrategy.h
│       │   ├── PurgeDispenserProcess.cpp
│       │   ├── PurgeDispenserProcess.h
│       │   ├── SupplyStabilizationPolicy.cpp
│       │   ├── SupplyStabilizationPolicy.h
│       │   ├── TriggerPlanner.cpp
│       │   ├── TriggerPlanner.h
│       │   ├── ValveCoordinationService.cpp
│       │   └── ValveCoordinationService.h
│       ├── model/
│       ├── planning/
│       │   └── domain-services/
│       │       ├── ContourOptimizationService.cpp
│       │       ├── ContourOptimizationService.h
│       │       ├── DispensingPlannerService.cpp
│       │       ├── DispensingPlannerService.h
│       │       ├── UnifiedTrajectoryPlannerService.cpp
│       │       └── UnifiedTrajectoryPlannerService.h
│       ├── ports/
│       │   ├── IDispensingExecutionObserver.h
│       │   ├── ITaskSchedulerPort.h
│       │   ├── ITriggerControllerPort.h
│       │   └── IValvePort.h
│       ├── simulation/
│       ├── value-objects/
│       │   ├── DispenseCompensationProfile.h
│       │   ├── DispensingExecutionTypes.h
│       │   ├── DispensingPlan.h
│       │   ├── QualityMetrics.h
│       │   ├── SafetyBoundary.h
│       │   └── TriggerPlan.h
│       └── CMakeLists.txt
├── CMakeLists.txt
└── module.yaml
```

**实现文件说明表**

**dispense-packaging/domain/dispensing/domain-services/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `ArcTriggerPointCalculator.cpp` | 运行时执行前的触发、阀控和时序封装阶段 | 当轨迹、阀控和触发条件已知，需要组装成执行包时触发。 | 计算或下发点胶触发时机，决定胶何时开始和停止输出。 | 输出触发点、阀动作时序、补偿结果或执行包片段，供运行时直接下发。 | 如果这里出错，现场会出现出胶时机错误、阀动作错误、补偿失效或执行包不可用。 |
| `CMPTriggerService.cpp` | 运行时执行前的触发、阀控和时序封装阶段 | 当轨迹、阀控和触发条件已知，需要组装成执行包时触发。 | 计算或下发点胶触发时机，决定胶何时开始和停止输出。 | 输出触发点、阀动作时序、补偿结果或执行包片段，供运行时直接下发。 | 如果这里出错，现场会出现出胶时机错误、阀动作错误、补偿失效或执行包不可用。 |
| `DispenseCompensationService.cpp` | 运行时执行前的触发、阀控和时序封装阶段 | 当轨迹、阀控和触发条件已知，需要组装成执行包时触发。 | 承担该环节中的主要行为实现。 | 输出触发点、阀动作时序、补偿结果或执行包片段，供运行时直接下发。 | 如果这里出错，现场会出现出胶时机错误、阀动作错误、补偿失效或执行包不可用。 |
| `DispensingController.cpp` | 运行时执行前的触发、阀控和时序封装阶段 | 当轨迹、阀控和触发条件已知，需要组装成执行包时触发。 | 把规划结果落实为受控动作或硬件指令。 | 输出触发点、阀动作时序、补偿结果或执行包片段，供运行时直接下发。 | 如果这里出错，现场会出现出胶时机错误、阀动作错误、补偿失效或执行包不可用。 |
| `DispensingProcessService.cpp` | 运行时执行前的触发、阀控和时序封装阶段 | 当轨迹、阀控和触发条件已知，需要组装成执行包时触发。 | 承担该环节中的主要行为实现。 | 输出触发点、阀动作时序、补偿结果或执行包片段，供运行时直接下发。 | 如果这里出错，现场会出现出胶时机错误、阀动作错误、补偿失效或执行包不可用。 |
| `PathOptimizationStrategy.cpp` | 运行时执行前的触发、阀控和时序封装阶段 | 当轨迹、阀控和触发条件已知，需要组装成执行包时触发。 | 承担该环节中的主要行为实现。 | 输出触发点、阀动作时序、补偿结果或执行包片段，供运行时直接下发。 | 如果这里出错，现场会出现出胶时机错误、阀动作错误、补偿失效或执行包不可用。 |
| `PurgeDispenserProcess.cpp` | 运行时执行前的触发、阀控和时序封装阶段 | 当轨迹、阀控和触发条件已知，需要组装成执行包时触发。 | 承担该环节中的主要行为实现。 | 输出触发点、阀动作时序、补偿结果或执行包片段，供运行时直接下发。 | 如果这里出错，现场会出现出胶时机错误、阀动作错误、补偿失效或执行包不可用。 |
| `SupplyStabilizationPolicy.cpp` | 运行时执行前的触发、阀控和时序封装阶段 | 当轨迹、阀控和触发条件已知，需要组装成执行包时触发。 | 承担该环节中的主要行为实现。 | 输出触发点、阀动作时序、补偿结果或执行包片段，供运行时直接下发。 | 如果这里出错，现场会出现出胶时机错误、阀动作错误、补偿失效或执行包不可用。 |
| `TriggerPlanner.cpp` | 运行时执行前的触发、阀控和时序封装阶段 | 当轨迹、阀控和触发条件已知，需要组装成执行包时触发。 | 计算或下发点胶触发时机，决定胶何时开始和停止输出。 | 输出触发点、阀动作时序、补偿结果或执行包片段，供运行时直接下发。 | 如果这里出错，现场会出现出胶时机错误、阀动作错误、补偿失效或执行包不可用。 |
| `ValveCoordinationService.cpp` | 运行时执行前的触发、阀控和时序封装阶段 | 当轨迹、阀控和触发条件已知，需要组装成执行包时触发。 | 控制阀的开闭和联动，直接影响出胶开始、保持和结束。 | 输出触发点、阀动作时序、补偿结果或执行包片段，供运行时直接下发。 | 如果这里出错，现场会出现出胶时机错误、阀动作错误、补偿失效或执行包不可用。 |

**dispense-packaging/domain/dispensing/planning/domain-services/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `ContourOptimizationService.cpp` | 执行包生成前的点胶规划细化阶段 | 当路径和运动结果已具备，需要继续细化点胶规划时触发。 | 承担该环节中的主要行为实现。 | 输出细化后的点胶规划结果，继续汇入执行包构建。 | 如果这里出错，现场会出现出胶时机错误、阀动作错误、补偿失效或执行包不可用。 |
| `DispensingPlannerService.cpp` | 执行包生成前的点胶规划细化阶段 | 当路径和运动结果已具备，需要继续细化点胶规划时触发。 | 把上游输入整理成下游可直接消费的规划结果。 | 输出细化后的点胶规划结果，继续汇入执行包构建。 | 如果这里出错，现场会出现出胶时机错误、阀动作错误、补偿失效或执行包不可用。 |
| `UnifiedTrajectoryPlannerService.cpp` | 执行包生成前的点胶规划细化阶段 | 当路径和运动结果已具备，需要继续细化点胶规划时触发。 | 把上游输入整理成下游可直接消费的规划结果。 | 输出细化后的点胶规划结果，继续汇入执行包构建。 | 如果这里出错，现场会出现出胶时机错误、阀动作错误、补偿失效或执行包不可用。 |

## dxf-geometry

- 模块编号：`M2`
- Owner 对象：`CanonicalGeometry`
- 所属分组：`upstream`

**文件树**
```text
dxf-geometry/
├── adapters/
│   └── dxf/
│       ├── AutoPathSourceAdapter.cpp
│       ├── AutoPathSourceAdapter.h
│       ├── CMakeLists.txt
│       ├── DXFAdapterFactory.cpp
│       ├── DXFAdapterFactory.h
│       ├── PbPathSourceAdapter.cpp
│       └── PbPathSourceAdapter.h
├── engineering-data/
│   ├── scripts/
│   │   ├── dxf_to_pb.py
│   │   ├── export_simulation_input.py
│   │   ├── generate_preview.py
│   │   └── path_to_trajectory.py
│   ├── src/
│   │   ├── engineering_data/
│   │   │   ├── cli/
│   │   │   │   ├── __init__.py
│   │   │   │   ├── dxf_to_pb.py
│   │   │   │   ├── export_simulation_input.py
│   │   │   │   ├── generate_preview.py
│   │   │   │   └── path_to_trajectory.py
│   │   │   ├── contracts/
│   │   │   │   ├── __init__.py
│   │   │   │   ├── preview.py
│   │   │   │   └── simulation_input.py
│   │   │   ├── models/
│   │   │   │   ├── __init__.py
│   │   │   │   ├── geometry.py
│   │   │   │   └── trajectory.py
│   │   │   ├── preview/
│   │   │   │   ├── __init__.py
│   │   │   │   └── html_preview.py
│   │   │   ├── processing/
│   │   │   │   ├── __init__.py
│   │   │   │   └── dxf_to_pb.py
│   │   │   ├── proto/
│   │   │   │   ├── __init__.py
│   │   │   │   └── dxf_primitives_pb2.py
│   │   │   ├── trajectory/
│   │   │   │   ├── __init__.py
│   │   │   │   └── offline_path_to_trajectory.py
│   │   │   └── __init__.py
│   │   ├── engineering_data.egg-info/
│   │   │   └── PKG-INFO
│   │   └── proto/
│   │       └── __init__.py
│   ├── tests/
│   │   ├── test_dxf_pipeline_legacy_exit.py
│   │   └── test_engineering_data_compatibility.py
│   └── pyproject.toml
├── src/
│   └── dxf/
│       ├── AutoPathSourceAdapter.cpp
│       ├── AutoPathSourceAdapter.h
│       ├── CMakeLists.txt
│       ├── DXFAdapterFactory.cpp
│       ├── DXFAdapterFactory.h
│       ├── PbPathSourceAdapter.cpp
│       └── PbPathSourceAdapter.h
├── CMakeLists.txt
└── module.yaml
```

**实现文件说明表**

**dxf-geometry/adapters/dxf/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `AutoPathSourceAdapter.cpp` | DXF 几何接入与转换阶段 | 当外部工程图形被导入系统并准备进入规划链时触发。 | 对 canonical `.pb` 输入做统一入口校验，并对 direct `.dxf` 输入执行 hard-fail guard。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |
| `DXFAdapterFactory.cpp` | DXF 几何接入与转换阶段 | 当外部工程图形被导入系统并准备进入规划链时触发。 | 组装 mock/local/remote DXF path source adapter，并把 local 行为收口到 canonical `.pb` 读取边界。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |
| `PbPathSourceAdapter.cpp` | DXF 几何接入与转换阶段 | 当外部工程图形被导入系统并准备进入规划链时触发。 | 从 `.pb` 解析运行时路径 primitive，作为 live 规划链唯一正式输入。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |

**dxf-geometry/engineering-data/scripts/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `dxf_to_pb.py` | 工程数据离线准备阶段 | 当工程数据需要离线转换、导出或预处理时触发。 | 承担该环节中的主要行为实现。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |
| `export_simulation_input.py` | 工程数据离线准备阶段 | 当工程数据需要离线转换、导出或预处理时触发。 | 承担该环节中的主要行为实现。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |
| `generate_preview.py` | 工程数据离线准备阶段 | 当工程数据需要离线转换、导出或预处理时触发。 | 输出可视化预览，便于在正式点胶前确认结果。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |
| `path_to_trajectory.py` | 工程数据离线准备阶段 | 当工程数据需要离线转换、导出或预处理时触发。 | 生成或调整轨迹，使运动控制器能够按预期走位。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |

**dxf-geometry/engineering-data/src/engineering_data/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `__init__.py` | DXF 几何接入与转换阶段 | 当外部工程图形被导入系统并准备进入规划链时触发。 | 承担该环节中的主要行为实现。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |

**dxf-geometry/engineering-data/src/engineering_data/cli/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `__init__.py` | 工程数据工具入口阶段 | 当外部工程图形被导入系统并准备进入规划链时触发。 | 承担该环节中的主要行为实现。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |
| `dxf_to_pb.py` | 工程数据工具入口阶段 | 当外部工程图形被导入系统并准备进入规划链时触发。 | 承担该环节中的主要行为实现。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |
| `export_simulation_input.py` | 工程数据工具入口阶段 | 当外部工程图形被导入系统并准备进入规划链时触发。 | 承担该环节中的主要行为实现。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |
| `generate_preview.py` | 工程数据工具入口阶段 | 当外部工程图形被导入系统并准备进入规划链时触发。 | 输出可视化预览，便于在正式点胶前确认结果。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |
| `path_to_trajectory.py` | 工程数据工具入口阶段 | 当外部工程图形被导入系统并准备进入规划链时触发。 | 生成或调整轨迹，使运动控制器能够按预期走位。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |

**dxf-geometry/engineering-data/src/engineering_data/contracts/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `__init__.py` | 几何数据交换口径整理阶段 | 当外部工程图形被导入系统并准备进入规划链时触发。 | 承担该环节中的主要行为实现。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |
| `preview.py` | 几何数据交换口径整理阶段 | 当外部工程图形被导入系统并准备进入规划链时触发。 | 输出可视化预览，便于在正式点胶前确认结果。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |
| `simulation_input.py` | 几何数据交换口径整理阶段 | 当外部工程图形被导入系统并准备进入规划链时触发。 | 承担该环节中的主要行为实现。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |

**dxf-geometry/engineering-data/src/engineering_data/models/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `__init__.py` | 几何事实建模阶段 | 当外部工程图形被导入系统并准备进入规划链时触发。 | 承担该环节中的主要行为实现。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |
| `geometry.py` | 几何事实建模阶段 | 当外部工程图形被导入系统并准备进入规划链时触发。 | 承担该环节中的主要行为实现。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |
| `trajectory.py` | 几何事实建模阶段 | 当外部工程图形被导入系统并准备进入规划链时触发。 | 生成或调整轨迹，使运动控制器能够按预期走位。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |

**dxf-geometry/engineering-data/src/engineering_data/preview/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `__init__.py` | 几何预览输出阶段 | 当需要在正式规划前查看几何预览效果时触发。 | 承担该环节中的主要行为实现。 | 输出几何预览结果，供人工确认。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |
| `html_preview.py` | 几何预览输出阶段 | 当需要在正式规划前查看几何预览效果时触发。 | 输出可视化预览，便于在正式点胶前确认结果。 | 输出几何预览结果，供人工确认。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |

**dxf-geometry/engineering-data/src/engineering_data/processing/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `__init__.py` | DXF 几何清洗与转换阶段 | 当 DXF 已接入、需要清洗和标准化为内部几何时触发。 | 承担该环节中的主要行为实现。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |
| `dxf_to_pb.py` | DXF 几何清洗与转换阶段 | 当 DXF 已接入、需要清洗和标准化为内部几何时触发。 | 承担该环节中的主要行为实现。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |

**dxf-geometry/engineering-data/src/engineering_data/proto/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `__init__.py` | 几何数据协议转换阶段 | 当外部工程图形被导入系统并准备进入规划链时触发。 | 承担该环节中的主要行为实现。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |
| `dxf_primitives_pb2.py` | 几何数据协议转换阶段 | 当外部工程图形被导入系统并准备进入规划链时触发。 | 承担该环节中的主要行为实现。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |

**dxf-geometry/engineering-data/src/engineering_data/trajectory/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `__init__.py` | 几何到轨迹的离线转换阶段 | 当离线流程需要从几何直接推导轨迹时触发。 | 承担该环节中的主要行为实现。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |
| `offline_path_to_trajectory.py` | 几何到轨迹的离线转换阶段 | 当离线流程需要从几何直接推导轨迹时触发。 | 生成或调整轨迹，使运动控制器能够按预期走位。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |

**dxf-geometry/engineering-data/src/proto/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `__init__.py` | DXF 几何接入与转换阶段 | 当外部工程图形被导入系统并准备进入规划链时触发。 | 承担该环节中的主要行为实现。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |

**dxf-geometry/engineering-data/tests/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `test_dxf_pipeline_legacy_exit.py` | 工程数据处理验证阶段 | 当验证几何处理链路时触发。 | 验证对应流程环节在正常和异常条件下是否符合预期。 | 输出验证结果，确认几何处理链路是否可靠。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |
| `test_engineering_data_compatibility.py` | 工程数据处理验证阶段 | 当验证几何处理链路时触发。 | 验证对应流程环节在正常和异常条件下是否符合预期。 | 输出验证结果，确认几何处理链路是否可靠。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |

**dxf-geometry/src/dxf/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `AutoPathSourceAdapter.cpp` | DXF 几何接入与转换阶段 | 当外部工程图形被导入系统并准备进入规划链时触发。 | 对 canonical `.pb` 输入做统一入口校验，并对 direct `.dxf` 输入执行 hard-fail guard。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |
| `DXFAdapterFactory.cpp` | DXF 几何接入与转换阶段 | 当外部工程图形被导入系统并准备进入规划链时触发。 | 组装 mock/local/remote DXF path source adapter，并把 local 行为收口到 canonical `.pb` 读取边界。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |
| `PbPathSourceAdapter.cpp` | DXF 几何接入与转换阶段 | 当外部工程图形被导入系统并准备进入规划链时触发。 | 从 `.pb` 解析运行时路径 primitive，作为 live 规划链唯一正式输入。 | 输出标准化几何、协议数据或离线轨迹数据，交给拓扑、规划或预览链继续使用。 | 如果这里出错，后面的特征提取、路径规划和出胶落点都会跟着失真。 |

## hmi-application

- 模块编号：`M11`
- Owner 对象：`UIViewModel`
- 所属分组：`runtime`
- 模块职责：HMI 任务接入、审批与状态查询的业务语义收敛。；人机展示对象与交互状态聚合规则。；HMI 域 owner 专属 contracts 与宿主调用边界。

**文件树**
```text
hmi-application/
├── src/
│   ├── CommandHandlers.cpp
│   ├── CommandHandlers.h
│   ├── CommandLineParser.cpp
│   └── CommandLineParser.h
├── CMakeLists.txt
└── module.yaml
```

**实现文件说明表**

**hmi-application/src/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `CommandHandlers.cpp` | 人机操作触发点胶流程的入口阶段 | 当操作员通过界面或命令发起点胶相关操作时触发。 | 接收外部命令并分发到对应点胶子流程。 | 输出应用层请求、界面状态或用户动作结果，交给工作流或运行时模块。 | 如果这里出错，操作员会触发错误流程或无法正确理解当前点胶状态。 |
| `CommandLineParser.cpp` | 人机操作触发点胶流程的入口阶段 | 当操作员通过界面或命令发起点胶相关操作时触发。 | 把外部输入解析成系统内部可执行的请求对象。 | 输出应用层请求、界面状态或用户动作结果，交给工作流或运行时模块。 | 如果这里出错，操作员会触发错误流程或无法正确理解当前点胶状态。 |

## job-ingest

- 模块编号：`M1`
- Owner 对象：`JobDefinition`
- 所属分组：`upstream`
- 模块职责：任务接入与输入归档（ingest）语义；输入校验与接入侧基础编排；面向上游静态工程链的输入事实边界

**文件树**
```text
job-ingest/
├── src/
│   ├── CommandHandlers.Connection.cpp
│   ├── CommandHandlers.cpp
│   ├── CommandHandlers.Dispensing.cpp
│   ├── CommandHandlers.Dxf.cpp
│   ├── CommandHandlers.h
│   ├── CommandHandlers.Motion.cpp
│   ├── CommandHandlers.Recipe.cpp
│   ├── CommandHandlersInternal.h
│   ├── CommandLineParser.cpp
│   ├── CommandLineParser.h
│   └── main.cpp
├── CMakeLists.txt
└── module.yaml
```

**实现文件说明表**

**job-ingest/src/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `CommandHandlers.Connection.cpp` | 点胶流程最前面的任务接入阶段 | 当外部系统、CLI 或 HMI 提交新的点胶任务或相关命令时触发。 | 接收外部命令并分发到对应点胶子流程。 | 输出内部任务对象、命令请求或作业事实，交给后续规划与编排链。 | 如果这里出错，后续规划和执行都会建立在错误任务上。 |
| `CommandHandlers.cpp` | 点胶流程最前面的任务接入阶段 | 当外部系统、CLI 或 HMI 提交新的点胶任务或相关命令时触发。 | 接收外部命令并分发到对应点胶子流程。 | 输出内部任务对象、命令请求或作业事实，交给后续规划与编排链。 | 如果这里出错，后续规划和执行都会建立在错误任务上。 |
| `CommandHandlers.Dispensing.cpp` | 点胶流程最前面的任务接入阶段 | 当外部系统、CLI 或 HMI 提交新的点胶任务或相关命令时触发。 | 接收外部命令并分发到对应点胶子流程。 | 输出内部任务对象、命令请求或作业事实，交给后续规划与编排链。 | 如果这里出错，后续规划和执行都会建立在错误任务上。 |
| `CommandHandlers.Dxf.cpp` | 点胶流程最前面的任务接入阶段 | 当外部系统、CLI 或 HMI 提交新的点胶任务或相关命令时触发。 | 接收外部命令并分发到对应点胶子流程。 | 输出内部任务对象、命令请求或作业事实，交给后续规划与编排链。 | 如果这里出错，后续规划和执行都会建立在错误任务上。 |
| `CommandHandlers.Motion.cpp` | 点胶流程最前面的任务接入阶段 | 当外部系统、CLI 或 HMI 提交新的点胶任务或相关命令时触发。 | 接收外部命令并分发到对应点胶子流程。 | 输出内部任务对象、命令请求或作业事实，交给后续规划与编排链。 | 如果这里出错，后续规划和执行都会建立在错误任务上。 |
| `CommandHandlers.Recipe.cpp` | 点胶流程最前面的任务接入阶段 | 当外部系统、CLI 或 HMI 提交新的点胶任务或相关命令时触发。 | 接收外部命令并分发到对应点胶子流程。 | 输出内部任务对象、命令请求或作业事实，交给后续规划与编排链。 | 如果这里出错，后续规划和执行都会建立在错误任务上。 |
| `CommandLineParser.cpp` | 点胶流程最前面的任务接入阶段 | 当外部系统、CLI 或 HMI 提交新的点胶任务或相关命令时触发。 | 把外部输入解析成系统内部可执行的请求对象。 | 输出内部任务对象、命令请求或作业事实，交给后续规划与编排链。 | 如果这里出错，后续规划和执行都会建立在错误任务上。 |
| `main.cpp` | 点胶流程最前面的任务接入阶段 | 当外部系统、CLI 或 HMI 提交新的点胶任务或相关命令时触发。 | 承担该环节中的主要行为实现。 | 输出内部任务对象、命令请求或作业事实，交给后续规划与编排链。 | 如果这里出错，后续规划和执行都会建立在错误任务上。 |

## motion-planning

- 模块编号：`M7`
- Owner 对象：`MotionTrajectory`
- 所属分组：`planning`
- 模块职责：运动规划对象与轨迹求解语义；规划链路中的运动约束建模与结果产出；通过 `motion_planning/contracts/*` 与 application facade 暴露稳定 public surface

**文件树**
```text
motion-planning/
├── domain/
│   └── motion/
│       ├── domain-services/
│       │   ├── interpolation/
│       │   │   ├── ArcGeometryMath.cpp
│       │   │   ├── ArcGeometryMath.h
│       │   │   ├── ArcInterpolator.cpp
│       │   │   ├── ArcInterpolator.h
│       │   │   ├── InterpolationCommandValidator.cpp
│       │   │   ├── InterpolationCommandValidator.h
│       │   │   ├── InterpolationProgramPlanner.cpp
│       │   │   ├── InterpolationProgramPlanner.h
│       │   │   ├── LinearInterpolator.cpp
│       │   │   ├── LinearInterpolator.h
│       │   │   ├── SplineGeometryMath.cpp
│       │   │   ├── SplineGeometryMath.h
│       │   │   ├── SplineInterpolator.cpp
│       │   │   ├── SplineInterpolator.h
│       │   │   ├── TrajectoryInterpolatorBase.cpp
│       │   │   ├── TrajectoryInterpolatorBase.h
│       │   │   ├── ValidatedInterpolationPort.cpp
│       │   │   └── ValidatedInterpolationPort.h
│       │   ├── GeometryBlender.cpp
│       │   ├── GeometryBlender.h
│       │   ├── MotionControlService.h
│       │   ├── MotionControlServiceImpl.cpp
│       │   ├── MotionStatusService.h
│       │   ├── MotionStatusServiceImpl.cpp
│       │   ├── MotionValidationService.h
│       │   ├── SevenSegmentSCurveProfile.cpp
│       │   ├── SevenSegmentSCurveProfile.h
│       │   ├── SpeedPlanner.cpp
│       │   ├── SpeedPlanner.h
│       │   ├── TimeTrajectoryPlanner.cpp
│       │   ├── TimeTrajectoryPlanner.h
│       │   ├── TrajectoryPlanner.cpp
│       │   ├── TrajectoryPlanner.h
│       │   ├── TriggerCalculator.cpp
│       │   ├── TriggerCalculator.h
│       │   ├── VelocityProfileService.cpp
│       │   └── VelocityProfileService.h
│       ├── ports/
│       │   ├── IAdvancedMotionPort.h
│       │   ├── IAxisControlPort.h
│       │   ├── IHomingPort.h
│       │   ├── IInterpolationPort.h
│       │   ├── IIOControlPort.h
│       │   ├── IJogControlPort.h
│       │   ├── IMotionConnectionPort.h
│       │   ├── IMotionRuntimePort.h
│       │   ├── IMotionStatePort.h
│       │   ├── IPositionControlPort.h
│       │   └── IVelocityProfilePort.h
│       ├── value-objects/
│       │   ├── HardwareTestTypes.h
│       │   ├── MotionTrajectory.h
│       │   ├── MotionTypes.h
│       │   ├── SemanticPath.h
│       │   ├── TimePlanningConfig.h
│       │   ├── TrajectoryAnalysisTypes.h
│       │   └── TrajectoryTypes.h
│       ├── BezierCalculator.cpp
│       ├── BezierCalculator.h
│       ├── BSplineCalculator.cpp
│       ├── BSplineCalculator.h
│       ├── CircleCalculator.cpp
│       ├── CircleCalculator.h
│       ├── CMakeLists.txt
│       ├── CMPCompensation.cpp
│       ├── CMPCompensation.h
│       ├── CMPCoordinatedInterpolator.cpp
│       ├── CMPCoordinatedInterpolator.h
│       ├── CMPValidator.cpp
│       └── CMPValidator.h
├── src/
│   └── motion/
│       ├── domain-services/
│       │   ├── interpolation/
│       │   │   ├── ArcGeometryMath.cpp
│       │   │   ├── ArcGeometryMath.h
│       │   │   ├── ArcInterpolator.cpp
│       │   │   ├── ArcInterpolator.h
│       │   │   ├── InterpolationCommandValidator.cpp
│       │   │   ├── InterpolationCommandValidator.h
│       │   │   ├── InterpolationProgramPlanner.cpp
│       │   │   ├── InterpolationProgramPlanner.h
│       │   │   ├── LinearInterpolator.cpp
│       │   │   ├── LinearInterpolator.h
│       │   │   ├── SplineGeometryMath.cpp
│       │   │   ├── SplineGeometryMath.h
│       │   │   ├── SplineInterpolator.cpp
│       │   │   ├── SplineInterpolator.h
│       │   │   ├── TrajectoryInterpolatorBase.cpp
│       │   │   ├── TrajectoryInterpolatorBase.h
│       │   │   ├── ValidatedInterpolationPort.cpp
│       │   │   └── ValidatedInterpolationPort.h
│       │   ├── GeometryBlender.cpp
│       │   ├── GeometryBlender.h
│       │   ├── HomingProcess.cpp
│       │   ├── HomingProcess.h
│       │   ├── JogController.cpp
│       │   ├── JogController.h
│       │   ├── MotionBufferController.cpp
│       │   ├── MotionBufferController.h
│       │   ├── MotionControlService.h
│       │   ├── MotionControlServiceImpl.cpp
│       │   ├── MotionControlServiceImpl.h
│       │   ├── MotionStatusService.h
│       │   ├── MotionStatusServiceImpl.cpp
│       │   ├── MotionStatusServiceImpl.h
│       │   ├── MotionValidationService.h
│       │   ├── SevenSegmentSCurveProfile.cpp
│       │   ├── SevenSegmentSCurveProfile.h
│       │   ├── SpeedPlanner.cpp
│       │   ├── SpeedPlanner.h
│       │   ├── TimeTrajectoryPlanner.cpp
│       │   ├── TimeTrajectoryPlanner.h
│       │   ├── TrajectoryPlanner.cpp
│       │   ├── TrajectoryPlanner.h
│       │   ├── TriggerCalculator.cpp
│       │   ├── TriggerCalculator.h
│       │   ├── VelocityProfileService.cpp
│       │   └── VelocityProfileService.h
│       ├── ports/
│       │   ├── IAdvancedMotionPort.h
│       │   ├── IAxisControlPort.h
│       │   ├── IHomingPort.h
│       │   ├── IInterpolationPort.h
│       │   ├── IIOControlPort.h
│       │   ├── IJogControlPort.h
│       │   ├── IMotionConnectionPort.h
│       │   ├── IMotionRuntimePort.h
│       │   ├── IMotionStatePort.h
│       │   ├── IPositionControlPort.h
│       │   └── IVelocityProfilePort.h
│       ├── value-objects/
│       │   ├── HardwareTestTypes.h
│       │   ├── MotionTrajectory.h
│       │   ├── MotionTypes.h
│       │   ├── SemanticPath.h
│       │   ├── TimePlanningConfig.h
│       │   ├── TrajectoryAnalysisTypes.h
│       │   └── TrajectoryTypes.h
│       ├── BezierCalculator.cpp
│       ├── BezierCalculator.h
│       ├── BSplineCalculator.cpp
│       ├── BSplineCalculator.h
│       ├── CircleCalculator.cpp
│       ├── CircleCalculator.h
│       ├── CMakeLists.txt
│       ├── CMPCompensation.cpp
│       ├── CMPCompensation.h
│       ├── CMPCoordinatedInterpolator.cpp
│       ├── CMPCoordinatedInterpolator.h
│       ├── CMPValidator.cpp
│       └── CMPValidator.h
├── CMakeLists.txt
└── module.yaml
```

**实现文件说明表**

**motion-planning/domain/motion/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `BezierCalculator.cpp` | 路径转运动轨迹的规划阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 承担该环节中的主要行为实现。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `BSplineCalculator.cpp` | 路径转运动轨迹的规划阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 承担该环节中的主要行为实现。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `CircleCalculator.cpp` | 路径转运动轨迹的规划阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 承担该环节中的主要行为实现。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `CMPCompensation.cpp` | 路径转运动轨迹的规划阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 承担该环节中的主要行为实现。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `CMPCoordinatedInterpolator.cpp` | 路径转运动轨迹的规划阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 承担该环节中的主要行为实现。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `CMPValidator.cpp` | 路径转运动轨迹的规划阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 承担该环节中的主要行为实现。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |

**motion-planning/domain/motion/domain-services/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `GeometryBlender.cpp` | 路径转运动轨迹的规划阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 承担该环节中的主要行为实现。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `MotionControlServiceImpl.cpp` | 路径转运动轨迹的规划阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 承担该环节中的主要行为实现。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `MotionStatusServiceImpl.cpp` | 路径转运动轨迹的规划阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 承担该环节中的主要行为实现。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `SevenSegmentSCurveProfile.cpp` | 路径转运动轨迹的规划阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 承担该环节中的主要行为实现。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `SpeedPlanner.cpp` | 路径转运动轨迹的规划阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 把上游输入整理成下游可直接消费的规划结果。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `TimeTrajectoryPlanner.cpp` | 路径转运动轨迹的规划阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 把上游输入整理成下游可直接消费的规划结果。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `TrajectoryPlanner.cpp` | 路径转运动轨迹的规划阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 把上游输入整理成下游可直接消费的规划结果。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `TriggerCalculator.cpp` | 路径转运动轨迹的规划阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 计算或下发点胶触发时机，决定胶何时开始和停止输出。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `VelocityProfileService.cpp` | 路径转运动轨迹的规划阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 承担该环节中的主要行为实现。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |

**motion-planning/domain/motion/domain-services/interpolation/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `ArcGeometryMath.cpp` | 运动规划内部的插补细化阶段 | 当轨迹已经形成，需要进一步细化为控制周期可消费的数据时触发。 | 承担该环节中的主要行为实现。 | 输出插补级控制数据，交给执行侧的运动控制器或运行时服务。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `ArcInterpolator.cpp` | 运动规划内部的插补细化阶段 | 当轨迹已经形成，需要进一步细化为控制周期可消费的数据时触发。 | 承担该环节中的主要行为实现。 | 输出插补级控制数据，交给执行侧的运动控制器或运行时服务。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `InterpolationCommandValidator.cpp` | 运动规划内部的插补细化阶段 | 当轨迹已经形成，需要进一步细化为控制周期可消费的数据时触发。 | 把轨迹细化成控制周期可消费的插补数据。 | 输出插补级控制数据，交给执行侧的运动控制器或运行时服务。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `InterpolationProgramPlanner.cpp` | 运动规划内部的插补细化阶段 | 当轨迹已经形成，需要进一步细化为控制周期可消费的数据时触发。 | 把上游输入整理成下游可直接消费的规划结果。 | 输出插补级控制数据，交给执行侧的运动控制器或运行时服务。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `LinearInterpolator.cpp` | 运动规划内部的插补细化阶段 | 当轨迹已经形成，需要进一步细化为控制周期可消费的数据时触发。 | 承担该环节中的主要行为实现。 | 输出插补级控制数据，交给执行侧的运动控制器或运行时服务。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `SplineGeometryMath.cpp` | 运动规划内部的插补细化阶段 | 当轨迹已经形成，需要进一步细化为控制周期可消费的数据时触发。 | 承担该环节中的主要行为实现。 | 输出插补级控制数据，交给执行侧的运动控制器或运行时服务。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `SplineInterpolator.cpp` | 运动规划内部的插补细化阶段 | 当轨迹已经形成，需要进一步细化为控制周期可消费的数据时触发。 | 承担该环节中的主要行为实现。 | 输出插补级控制数据，交给执行侧的运动控制器或运行时服务。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `TrajectoryInterpolatorBase.cpp` | 运动规划内部的插补细化阶段 | 当轨迹已经形成，需要进一步细化为控制周期可消费的数据时触发。 | 生成或调整轨迹，使运动控制器能够按预期走位。 | 输出插补级控制数据，交给执行侧的运动控制器或运行时服务。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `ValidatedInterpolationPort.cpp` | 运动规划内部的插补细化阶段 | 当轨迹已经形成，需要进一步细化为控制周期可消费的数据时触发。 | 把轨迹细化成控制周期可消费的插补数据。 | 输出插补级控制数据，交给执行侧的运动控制器或运行时服务。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |

**motion-planning/src/motion/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `BezierCalculator.cpp` | 运动规划的桥接兼容阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 承担该环节中的主要行为实现。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `BSplineCalculator.cpp` | 运动规划的桥接兼容阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 承担该环节中的主要行为实现。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `CircleCalculator.cpp` | 运动规划的桥接兼容阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 承担该环节中的主要行为实现。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `CMPCompensation.cpp` | 运动规划的桥接兼容阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 承担该环节中的主要行为实现。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `CMPCoordinatedInterpolator.cpp` | 运动规划的桥接兼容阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 承担该环节中的主要行为实现。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `CMPValidator.cpp` | 运动规划的桥接兼容阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 承担该环节中的主要行为实现。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |

**motion-planning/src/motion/domain-services/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `GeometryBlender.cpp` | 运动规划的桥接兼容阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 承担该环节中的主要行为实现。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `HomingProcess.cpp` | 运动规划的桥接兼容阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 执行回零与基准建立，确保正式点胶前的起始坐标正确。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `JogController.cpp` | 运动规划的桥接兼容阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 支持人工点动和微调，用于调试、对位或故障处理。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `MotionBufferController.cpp` | 运动规划的桥接兼容阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 把规划结果落实为受控动作或硬件指令。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `MotionControlServiceImpl.cpp` | 运动规划的桥接兼容阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 承担该环节中的主要行为实现。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `MotionStatusServiceImpl.cpp` | 运动规划的桥接兼容阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 承担该环节中的主要行为实现。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `SevenSegmentSCurveProfile.cpp` | 运动规划的桥接兼容阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 承担该环节中的主要行为实现。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `SpeedPlanner.cpp` | 运动规划的桥接兼容阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 把上游输入整理成下游可直接消费的规划结果。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `TimeTrajectoryPlanner.cpp` | 运动规划的桥接兼容阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 把上游输入整理成下游可直接消费的规划结果。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `TrajectoryPlanner.cpp` | 运动规划的桥接兼容阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 把上游输入整理成下游可直接消费的规划结果。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `TriggerCalculator.cpp` | 运动规划的桥接兼容阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 计算或下发点胶触发时机，决定胶何时开始和停止输出。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `VelocityProfileService.cpp` | 运动规划的桥接兼容阶段 | 当工艺路径已经生成，需要转成运动控制可执行轨迹时触发。 | 承担该环节中的主要行为实现。 | 输出轨迹、速度曲线或运动控制相关结果，交给点胶封装与现场执行阶段。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |

**motion-planning/src/motion/domain-services/interpolation/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `ArcGeometryMath.cpp` | 运动规划内部的插补细化阶段 | 当轨迹已经形成，需要进一步细化为控制周期可消费的数据时触发。 | 承担该环节中的主要行为实现。 | 输出插补级控制数据，交给执行侧的运动控制器或运行时服务。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `ArcInterpolator.cpp` | 运动规划内部的插补细化阶段 | 当轨迹已经形成，需要进一步细化为控制周期可消费的数据时触发。 | 承担该环节中的主要行为实现。 | 输出插补级控制数据，交给执行侧的运动控制器或运行时服务。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `InterpolationCommandValidator.cpp` | 运动规划内部的插补细化阶段 | 当轨迹已经形成，需要进一步细化为控制周期可消费的数据时触发。 | 把轨迹细化成控制周期可消费的插补数据。 | 输出插补级控制数据，交给执行侧的运动控制器或运行时服务。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `InterpolationProgramPlanner.cpp` | 运动规划内部的插补细化阶段 | 当轨迹已经形成，需要进一步细化为控制周期可消费的数据时触发。 | 把上游输入整理成下游可直接消费的规划结果。 | 输出插补级控制数据，交给执行侧的运动控制器或运行时服务。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `LinearInterpolator.cpp` | 运动规划内部的插补细化阶段 | 当轨迹已经形成，需要进一步细化为控制周期可消费的数据时触发。 | 承担该环节中的主要行为实现。 | 输出插补级控制数据，交给执行侧的运动控制器或运行时服务。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `SplineGeometryMath.cpp` | 运动规划内部的插补细化阶段 | 当轨迹已经形成，需要进一步细化为控制周期可消费的数据时触发。 | 承担该环节中的主要行为实现。 | 输出插补级控制数据，交给执行侧的运动控制器或运行时服务。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `SplineInterpolator.cpp` | 运动规划内部的插补细化阶段 | 当轨迹已经形成，需要进一步细化为控制周期可消费的数据时触发。 | 承担该环节中的主要行为实现。 | 输出插补级控制数据，交给执行侧的运动控制器或运行时服务。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `TrajectoryInterpolatorBase.cpp` | 运动规划内部的插补细化阶段 | 当轨迹已经形成，需要进一步细化为控制周期可消费的数据时触发。 | 生成或调整轨迹，使运动控制器能够按预期走位。 | 输出插补级控制数据，交给执行侧的运动控制器或运行时服务。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |
| `ValidatedInterpolationPort.cpp` | 运动规划内部的插补细化阶段 | 当轨迹已经形成，需要进一步细化为控制周期可消费的数据时触发。 | 把轨迹细化成控制周期可消费的插补数据。 | 输出插补级控制数据，交给执行侧的运动控制器或运行时服务。 | 如果这里出错，现场会表现为轨迹不平滑、速度异常、抖动或定位偏差。 |

## process-path

- 模块编号：`M6`
- Owner 对象：`ProcessPath`
- 所属分组：`planning`
- 模块职责：基于 `CoordinateTransformSet` 与上游规则构建 `ProcessPath`。；承接路径段、路径序列与路径校验的模块 owner 边界。；为 `M7 motion-planning` 提供正式路径事实，不回退为执行层的临时中间态。

**文件树**
```text
process-path/
├── domain/
│   └── trajectory/
│       ├── domain-services/
│       │   ├── GeometryNormalizer.cpp
│       │   ├── GeometryNormalizer.h
│       │   ├── MotionPlanner.cpp
│       │   ├── MotionPlanner.h
│       │   ├── PathRegularizer.cpp
│       │   ├── PathRegularizer.h
│       │   ├── ProcessAnnotator.cpp
│       │   ├── ProcessAnnotator.h
│       │   ├── SplineApproximation.cpp
│       │   ├── SplineApproximation.h
│       │   ├── TrajectoryShaper.cpp
│       │   └── TrajectoryShaper.h
│       ├── ports/
│       │   ├── IDXFPathSourcePort.h
│       │   └── IPathSourcePort.h
│       ├── value-objects/
│       │   ├── GeometryBoostAdapter.h
│       │   ├── GeometryUtils.h
│       │   ├── MotionConfig.h
│       │   ├── Path.h
│       │   ├── PlanningReport.h
│       │   ├── Primitive.h
│       │   ├── ProcessConfig.h
│       │   ├── ProcessPath.h
│       │   └── Trajectory.h
│       └── CMakeLists.txt
├── CMakeLists.txt
└── module.yaml
```

**实现文件说明表**

**process-path/domain/trajectory/domain-services/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `GeometryNormalizer.cpp` | 工艺规划之后、运动规划之前的路径生成阶段 | 当工艺规划结果和坐标对齐结果准备完成，需要形成正式加工路径时触发。 | 承担该环节中的主要行为实现。 | 输出正式的加工路径、路径段或路径校验结果，交给运动规划继续处理。 | 如果这里出错，后续运动规划会沿着错误路径执行，直接影响轨迹和落点。 |
| `MotionPlanner.cpp` | 工艺规划之后、运动规划之前的路径生成阶段 | 当工艺规划结果和坐标对齐结果准备完成，需要形成正式加工路径时触发。 | 把上游输入整理成下游可直接消费的规划结果。 | 输出正式的加工路径、路径段或路径校验结果，交给运动规划继续处理。 | 如果这里出错，后续运动规划会沿着错误路径执行，直接影响轨迹和落点。 |
| `PathRegularizer.cpp` | 工艺规划之后、运动规划之前的路径生成阶段 | 当工艺规划结果和坐标对齐结果准备完成，需要形成正式加工路径时触发。 | 承担该环节中的主要行为实现。 | 输出正式的加工路径、路径段或路径校验结果，交给运动规划继续处理。 | 如果这里出错，后续运动规划会沿着错误路径执行，直接影响轨迹和落点。 |
| `ProcessAnnotator.cpp` | 工艺规划之后、运动规划之前的路径生成阶段 | 当工艺规划结果和坐标对齐结果准备完成，需要形成正式加工路径时触发。 | 承担该环节中的主要行为实现。 | 输出正式的加工路径、路径段或路径校验结果，交给运动规划继续处理。 | 如果这里出错，后续运动规划会沿着错误路径执行，直接影响轨迹和落点。 |
| `SplineApproximation.cpp` | 工艺规划之后、运动规划之前的路径生成阶段 | 当工艺规划结果和坐标对齐结果准备完成，需要形成正式加工路径时触发。 | 承担该环节中的主要行为实现。 | 输出正式的加工路径、路径段或路径校验结果，交给运动规划继续处理。 | 如果这里出错，后续运动规划会沿着错误路径执行，直接影响轨迹和落点。 |
| `TrajectoryShaper.cpp` | 工艺规划之后、运动规划之前的路径生成阶段 | 当工艺规划结果和坐标对齐结果准备完成，需要形成正式加工路径时触发。 | 生成或调整轨迹，使运动控制器能够按预期走位。 | 输出正式的加工路径、路径段或路径校验结果，交给运动规划继续处理。 | 如果这里出错，后续运动规划会沿着错误路径执行，直接影响轨迹和落点。 |

## process-planning

- 模块编号：`M4`
- Owner 对象：`ProcessPlan`
- 所属分组：`planning`
- 模块职责：工艺规划规则编排与 `ProcessPlan` 事实收敛。；承接 `FeatureGraph -> ProcessPlan` 的规划阶段转换语义。；为 `M5-M8` 提供规划链上游输出，不越权承担运行时执行职责。

**文件树**
```text
process-planning/
├── domain/
│   └── configuration/
│       ├── ports/
│       │   ├── IConfigurationPort.h
│       │   ├── IFileStoragePort.h
│       │   └── ValveConfig.h
│       ├── value-objects/
│       │   └── ConfigTypes.h
│       ├── CMakeLists.txt
│       ├── CMPPulseConfig.cpp
│       ├── DispensingConfig.cpp
│       ├── InterpolationConfig.cpp
│       ├── MachineConfig.cpp
│       └── ValveTimingConfig.cpp
├── CMakeLists.txt
└── module.yaml
```

**实现文件说明表**

**process-planning/domain/configuration/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `CMPPulseConfig.cpp` | 点胶前的工艺参数收敛阶段 | 当任务、配方和设备参数已经到位，需要生成工艺约束时触发。 | 收敛设备、工艺或运行参数，为后续规划和执行提供边界条件。 | 输出工艺参数和配置约束，供路径生成、运动规划和点胶封装阶段消费。 | 如果这里出错，后续路径、速度和出胶策略会建立在错误参数上。 |
| `DispensingConfig.cpp` | 点胶前的工艺参数收敛阶段 | 当任务、配方和设备参数已经到位，需要生成工艺约束时触发。 | 收敛设备、工艺或运行参数，为后续规划和执行提供边界条件。 | 输出工艺参数和配置约束，供路径生成、运动规划和点胶封装阶段消费。 | 如果这里出错，后续路径、速度和出胶策略会建立在错误参数上。 |
| `InterpolationConfig.cpp` | 点胶前的工艺参数收敛阶段 | 当任务、配方和设备参数已经到位，需要生成工艺约束时触发。 | 收敛设备、工艺或运行参数，为后续规划和执行提供边界条件。 | 输出工艺参数和配置约束，供路径生成、运动规划和点胶封装阶段消费。 | 如果这里出错，后续路径、速度和出胶策略会建立在错误参数上。 |
| `MachineConfig.cpp` | 点胶前的工艺参数收敛阶段 | 当任务、配方和设备参数已经到位，需要生成工艺约束时触发。 | 收敛设备、工艺或运行参数，为后续规划和执行提供边界条件。 | 输出工艺参数和配置约束，供路径生成、运动规划和点胶封装阶段消费。 | 如果这里出错，后续路径、速度和出胶策略会建立在错误参数上。 |
| `ValveTimingConfig.cpp` | 点胶前的工艺参数收敛阶段 | 当任务、配方和设备参数已经到位，需要生成工艺约束时触发。 | 收敛设备、工艺或运行参数，为后续规划和执行提供边界条件。 | 输出工艺参数和配置约束，供路径生成、运动规划和点胶封装阶段消费。 | 如果这里出错，后续路径、速度和出胶策略会建立在错误参数上。 |

## runtime-execution

- 模块编号：`M9`
- Owner 对象：`ExecutionSession`
- 所属分组：`runtime`
- 模块职责：运行时执行链调度、执行状态收敛与失败归责语义。；消费 `M4-M8` 规划结果并驱动执行，不回写上游规划事实。；汇聚运行时执行域的 owner 专属 contracts 与 adapters 边界。

**文件树**
```text
runtime-execution/
├── adapters/
│   └── device/
│       ├── include/
│       │   └── siligen/
│       │       └── device/
│       │           └── adapters/
│       │               ├── dispensing/
│       │               │   ├── TriggerControllerAdapter.h
│       │               │   └── ValveAdapter.h
│       │               ├── drivers/
│       │               │   └── multicard/
│       │               │       ├── error_mapper.h
│       │               │       ├── IMultiCardWrapper.h
│       │               │       ├── MockMultiCard.h
│       │               │       ├── MockMultiCardWrapper.h
│       │               │       ├── MultiCardCPP.h
│       │               │       └── RealMultiCardWrapper.h
│       │               ├── fake/
│       │               │   ├── fake_dispenser_device.h
│       │               │   └── fake_motion_device.h
│       │               ├── hardware/
│       │               │   ├── HardwareConnectionAdapter.h
│       │               │   └── HardwareTestAdapter.h
│       │               ├── io/
│       │               │   └── multicard_digital_io_adapter.h
│       │               └── motion/
│       │                   ├── HomingPortAdapter.h
│       │                   ├── HomingSupport.h
│       │                   ├── InterpolationAdapter.h
│       │                   ├── MotionRuntimeConnectionAdapter.h
│       │                   ├── MotionRuntimeFacade.h
│       │                   ├── multicard_motion_device.h
│       │                   └── MultiCardMotionAdapter.h
│       ├── src/
│       │   ├── drivers/
│       │   │   └── multicard/
│       │   │       ├── error_mapper.cpp
│       │   │       ├── MockMultiCard.cpp
│       │   │       ├── MockMultiCardWrapper.cpp
│       │   │       └── RealMultiCardWrapper.cpp
│       │   ├── fake/
│       │   │   ├── fake_dispenser_device.cpp
│       │   │   └── fake_motion_device.cpp
│       │   ├── io/
│       │   │   └── multicard_digital_io_adapter.cpp
│       │   ├── legacy/
│       │   │   ├── adapters/
│       │   │   │   ├── diagnostics/
│       │   │   │   │   └── health/
│       │   │   │   │       └── testing/
│       │   │   │   │           ├── HardwareTestAdapter.Cmp.cpp
│       │   │   │   │           ├── HardwareTestAdapter.Connection.cpp
│       │   │   │   │           ├── HardwareTestAdapter.cpp
│       │   │   │   │           ├── HardwareTestAdapter.Diagnostics.cpp
│       │   │   │   │           ├── HardwareTestAdapter.h
│       │   │   │   │           ├── HardwareTestAdapter.Helpers.cpp
│       │   │   │   │           ├── HardwareTestAdapter.Homing.cpp
│       │   │   │   │           ├── HardwareTestAdapter.Interpolation.cpp
│       │   │   │   │           ├── HardwareTestAdapter.IO.cpp
│       │   │   │   │           ├── HardwareTestAdapter.Jog.cpp
│       │   │   │   │           ├── HardwareTestAdapter.Safety.cpp
│       │   │   │   │           └── HardwareTestAdapter.Trigger.cpp
│       │   │   │   ├── dispensing/
│       │   │   │   │   └── dispenser/
│       │   │   │   │       ├── triggering/
│       │   │   │   │       │   ├── TriggerControllerAdapter.cpp
│       │   │   │   │       │   └── TriggerControllerAdapter.h
│       │   │   │   │       ├── ValveAdapter.cpp
│       │   │   │   │       ├── ValveAdapter.Dispenser.cpp
│       │   │   │   │       ├── ValveAdapter.h
│       │   │   │   │       ├── ValveAdapter.Hardware.cpp
│       │   │   │   │       └── ValveAdapter.Supply.cpp
│       │   │   │   └── motion/
│       │   │   │       └── controller/
│       │   │   │           ├── connection/
│       │   │   │           │   ├── HardwareConnectionAdapter.cpp
│       │   │   │           │   └── HardwareConnectionAdapter.h
│       │   │   │           ├── control/
│       │   │   │           │   ├── MultiCardMotionAdapter.cpp
│       │   │   │           │   ├── MultiCardMotionAdapter.h
│       │   │   │           │   ├── MultiCardMotionAdapter.Helpers.cpp
│       │   │   │           │   ├── MultiCardMotionAdapter.Motion.cpp
│       │   │   │           │   ├── MultiCardMotionAdapter.Settings.cpp
│       │   │   │           │   └── MultiCardMotionAdapter.Status.cpp
│       │   │   │           ├── homing/
│       │   │   │           │   ├── HomingPortAdapter.cpp
│       │   │   │           │   ├── HomingPortAdapter.h
│       │   │   │           │   └── HomingSupport.h
│       │   │   │           ├── internal/
│       │   │   │           │   ├── UnitConverter.cpp
│       │   │   │           │   └── UnitConverter.h
│       │   │   │           ├── interpolation/
│       │   │   │           │   ├── InterpolationAdapter.cpp
│       │   │   │           │   └── InterpolationAdapter.h
│       │   │   │           └── runtime/
│       │   │   │               ├── MotionRuntimeConnectionAdapter.cpp
│       │   │   │               ├── MotionRuntimeConnectionAdapter.h
│       │   │   │               ├── MotionRuntimeFacade.cpp
│       │   │   │               └── MotionRuntimeFacade.h
│       │   │   └── drivers/
│       │   │       └── multicard/
│       │   │           ├── MultiCardAdapter.cpp
│       │   │           ├── MultiCardAdapter.h
│       │   │           └── MultiCardStub_Class.cpp
│       │   └── motion/
│       │       └── multicard_motion_device.cpp
│       └── CMakeLists.txt
├── contracts/
│   └── device/
│       ├── include/
│       │   └── siligen/
│       │       └── device/
│       │           └── contracts/
│       │               ├── capabilities/
│       │               │   └── device_capabilities.h
│       │               ├── commands/
│       │               │   └── device_commands.h
│       │               ├── events/
│       │               │   └── device_events.h
│       │               ├── faults/
│       │               │   └── device_faults.h
│       │               ├── ports/
│       │               │   └── device_ports.h
│       │               ├── state/
│       │               │   └── device_state.h
│       │               └── device_contracts.h
│       └── CMakeLists.txt
├── device-adapters/
│   └── CMakeLists.txt
├── device-contracts/
│   └── CMakeLists.txt
├── runtime/
│   └── host/
│       ├── bootstrap/
│       │   ├── InfrastructureBindings.h
│       │   └── InfrastructureBindingsBuilder.cpp
│       ├── container/
│       │   ├── ApplicationContainer.cpp
│       │   ├── ApplicationContainer.Dispensing.cpp
│       │   ├── ApplicationContainer.h
│       │   ├── ApplicationContainer.Motion.cpp
│       │   ├── ApplicationContainer.Recipes.cpp
│       │   ├── ApplicationContainer.System.cpp
│       │   └── ApplicationContainerFwd.h
│       ├── factories/
│       │   ├── InfrastructureAdapterFactory.cpp
│       │   └── InfrastructureAdapterFactory.h
│       ├── runtime/
│       │   ├── configuration/
│       │   │   ├── validators/
│       │   │   │   ├── ConfigValidator.cpp
│       │   │   │   └── ConfigValidator.h
│       │   │   ├── ConfigFileAdapter.cpp
│       │   │   ├── ConfigFileAdapter.h
│       │   │   ├── ConfigFileAdapter.Ini.cpp
│       │   │   ├── ConfigFileAdapter.Sections.cpp
│       │   │   ├── ConfigFileAdapter.System.cpp
│       │   │   ├── InterlockConfigResolver.cpp
│       │   │   ├── InterlockConfigResolver.h
│       │   │   └── WorkspaceAssetPaths.h
│       │   ├── diagnostics/
│       │   │   ├── DiagnosticsPortAdapter.cpp
│       │   │   └── DiagnosticsPortAdapter.h
│       │   ├── events/
│       │   │   ├── CMakeLists.txt
│       │   │   ├── InMemoryEventPublisherAdapter.cpp
│       │   │   └── InMemoryEventPublisherAdapter.h
│       │   ├── recipes/
│       │   │   ├── AuditFileRepository.cpp
│       │   │   ├── AuditFileRepository.h
│       │   │   ├── ParameterSchemaFileProvider.cpp
│       │   │   ├── ParameterSchemaFileProvider.h
│       │   │   ├── RecipeBundleSerializer.cpp
│       │   │   ├── RecipeBundleSerializer.h
│       │   │   ├── RecipeFileRepository.cpp
│       │   │   ├── RecipeFileRepository.h
│       │   │   ├── TemplateFileRepository.cpp
│       │   │   └── TemplateFileRepository.h
│       │   ├── scheduling/
│       │   │   ├── CMakeLists.txt
│       │   │   ├── TaskSchedulerAdapter.cpp
│       │   │   └── TaskSchedulerAdapter.h
│       │   └── storage/
│       │       └── files/
│       │           ├── LocalFileStorageAdapter.cpp
│       │           └── LocalFileStorageAdapter.h
│       ├── security/
│       │   ├── AuditLogger.cpp
│       │   ├── AuditLogger.h
│       │   ├── InterlockMonitor.cpp
│       │   ├── InterlockMonitor.h
│       │   ├── SecurityLogHelper.h
│       │   └── SecurityLogHelper.h
│       ├── services/
│       │   └── motion/
│       │       ├── HardLimitMonitorService.cpp
│       │       ├── HardLimitMonitorService.h
│       │       ├── SoftLimitMonitorService.cpp
│       │       └── SoftLimitMonitorService.h
│       ├── tests/
│       │   ├── unit/
│       │   │   └── runtime/
│       │   │       ├── configuration/
│       │   │       │   ├── ConfigFileAdapterHardwareConfigurationTest.cpp
│       │   │       │   ├── InterlockConfigResolverTest.cpp
│       │   │       │   └── WorkspaceAssetPathsTest.cpp
│       │   │       ├── motion/
│       │   │       │   └── MotionRuntimeFacadeTest.cpp
│       │   │       └── recipes/
│       │   │           └── TemplateFileRepositoryTest.cpp
│       │   └── CMakeLists.txt
│       ├── CMakeLists.txt
│       ├── ContainerBootstrap.cpp
│       └── ContainerBootstrap.h
├── runtime-host/
│   ├── tests/
│   │   └── CMakeLists.txt
│   └── CMakeLists.txt
├── tests/
│   └── runtime-host/
│       └── CMakeLists.txt
├── CMakeLists.txt
└── module.yaml
```

**实现文件说明表**

**runtime-execution/adapters/device/src/drivers/multicard/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `error_mapper.cpp` | 现场执行最底层的板卡驱动阶段 | 当更底层的板卡或驱动接口需要被调用时触发。 | 承担该环节中的主要行为实现。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `MockMultiCard.cpp` | 现场执行最底层的板卡驱动阶段 | 当更底层的板卡或驱动接口需要被调用时触发。 | 承担该环节中的主要行为实现。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `MockMultiCardWrapper.cpp` | 现场执行最底层的板卡驱动阶段 | 当更底层的板卡或驱动接口需要被调用时触发。 | 承担该环节中的主要行为实现。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `RealMultiCardWrapper.cpp` | 现场执行最底层的板卡驱动阶段 | 当更底层的板卡或驱动接口需要被调用时触发。 | 承担该环节中的主要行为实现。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/adapters/device/src/fake/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `fake_dispenser_device.cpp` | 现场执行的仿真设备阶段 | 当使用仿真或测试替身设备而非真实硬件时触发。 | 承担该环节中的主要行为实现。 | 输出运行时会话、调度结果或执行状态，供现场执行链继续推进。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `fake_motion_device.cpp` | 现场执行的仿真设备阶段 | 当使用仿真或测试替身设备而非真实硬件时触发。 | 承担该环节中的主要行为实现。 | 输出运行时会话、调度结果或执行状态，供现场执行链继续推进。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/adapters/device/src/io/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `multicard_digital_io_adapter.cpp` | 现场执行的 IO 下发阶段 | 当需要下发数字 IO 指令到现场设备时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/adapters/device/src/legacy/adapters/diagnostics/health/testing/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `HardwareTestAdapter.Cmp.cpp` | 现场执行过程中的事件与状态回收阶段 | 当现场执行过程中产生状态变化、事件或异常时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出运行事件、诊断记录或状态变化，供监控与追溯使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `HardwareTestAdapter.Connection.cpp` | 现场执行过程中的事件与状态回收阶段 | 当现场执行过程中产生状态变化、事件或异常时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出运行事件、诊断记录或状态变化，供监控与追溯使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `HardwareTestAdapter.cpp` | 现场执行过程中的事件与状态回收阶段 | 当现场执行过程中产生状态变化、事件或异常时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出运行事件、诊断记录或状态变化，供监控与追溯使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `HardwareTestAdapter.Diagnostics.cpp` | 现场执行过程中的事件与状态回收阶段 | 当现场执行过程中产生状态变化、事件或异常时触发。 | 记录执行过程中的关键事件和异常，支撑问题追溯。 | 输出运行事件、诊断记录或状态变化，供监控与追溯使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `HardwareTestAdapter.Helpers.cpp` | 现场执行过程中的事件与状态回收阶段 | 当现场执行过程中产生状态变化、事件或异常时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出运行事件、诊断记录或状态变化，供监控与追溯使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `HardwareTestAdapter.Homing.cpp` | 现场执行过程中的事件与状态回收阶段 | 当现场执行过程中产生状态变化、事件或异常时触发。 | 执行回零与基准建立，确保正式点胶前的起始坐标正确。 | 输出运行事件、诊断记录或状态变化，供监控与追溯使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `HardwareTestAdapter.Interpolation.cpp` | 现场执行过程中的事件与状态回收阶段 | 当现场执行过程中产生状态变化、事件或异常时触发。 | 把轨迹细化成控制周期可消费的插补数据。 | 输出运行事件、诊断记录或状态变化，供监控与追溯使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `HardwareTestAdapter.IO.cpp` | 现场执行过程中的事件与状态回收阶段 | 当现场执行过程中产生状态变化、事件或异常时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出运行事件、诊断记录或状态变化，供监控与追溯使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `HardwareTestAdapter.Jog.cpp` | 现场执行过程中的事件与状态回收阶段 | 当现场执行过程中产生状态变化、事件或异常时触发。 | 支持人工点动和微调，用于调试、对位或故障处理。 | 输出运行事件、诊断记录或状态变化，供监控与追溯使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `HardwareTestAdapter.Safety.cpp` | 现场执行过程中的事件与状态回收阶段 | 当现场执行过程中产生状态变化、事件或异常时触发。 | 执行安全联锁或限位判断，防止设备在危险条件下继续动作。 | 输出运行事件、诊断记录或状态变化，供监控与追溯使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `HardwareTestAdapter.Trigger.cpp` | 现场执行过程中的事件与状态回收阶段 | 当现场执行过程中产生状态变化、事件或异常时触发。 | 计算或下发点胶触发时机，决定胶何时开始和停止输出。 | 输出运行事件、诊断记录或状态变化，供监控与追溯使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/adapters/device/src/legacy/adapters/dispensing/dispenser/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `ValveAdapter.cpp` | 现场点胶执行时的阀和点胶头控制阶段 | 当执行包进入现场动作阶段，需要真正控制阀或点胶头时触发。 | 控制阀的开闭和联动，直接影响出胶开始、保持和结束。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `ValveAdapter.Dispenser.cpp` | 现场点胶执行时的阀和点胶头控制阶段 | 当执行包进入现场动作阶段，需要真正控制阀或点胶头时触发。 | 控制阀的开闭和联动，直接影响出胶开始、保持和结束。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `ValveAdapter.Hardware.cpp` | 现场点胶执行时的阀和点胶头控制阶段 | 当执行包进入现场动作阶段，需要真正控制阀或点胶头时触发。 | 控制阀的开闭和联动，直接影响出胶开始、保持和结束。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `ValveAdapter.Supply.cpp` | 现场点胶执行时的阀和点胶头控制阶段 | 当执行包进入现场动作阶段，需要真正控制阀或点胶头时触发。 | 控制阀的开闭和联动，直接影响出胶开始、保持和结束。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/adapters/device/src/legacy/adapters/dispensing/dispenser/triggering/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `TriggerControllerAdapter.cpp` | 现场点胶执行时的触发信号下发阶段 | 当运行时到达某个路径位置或条件，需要发出触发信号开始/停止出胶时触发。 | 计算或下发点胶触发时机，决定胶何时开始和停止输出。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/adapters/device/src/legacy/adapters/motion/controller/connection/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `HardwareConnectionAdapter.cpp` | 执行前的控制器连接建立阶段 | 当执行前需要与运动控制器建立连接或确认在线状态时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/adapters/device/src/legacy/adapters/motion/controller/control/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `MultiCardMotionAdapter.cpp` | 现场执行时的运动控制命令下发阶段 | 当现场执行需要发送运动控制指令时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `MultiCardMotionAdapter.Helpers.cpp` | 现场执行时的运动控制命令下发阶段 | 当现场执行需要发送运动控制指令时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `MultiCardMotionAdapter.Motion.cpp` | 现场执行时的运动控制命令下发阶段 | 当现场执行需要发送运动控制指令时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `MultiCardMotionAdapter.Settings.cpp` | 现场执行时的运动控制命令下发阶段 | 当现场执行需要发送运动控制指令时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `MultiCardMotionAdapter.Status.cpp` | 现场执行时的运动控制命令下发阶段 | 当现场执行需要发送运动控制指令时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/adapters/device/src/legacy/adapters/motion/controller/homing/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `HomingPortAdapter.cpp` | 正式点胶前的回零准备阶段 | 当正式点胶前需要回零或建立设备基准位置时触发。 | 执行回零与基准建立，确保正式点胶前的起始坐标正确。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/adapters/device/src/legacy/adapters/motion/controller/internal/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `UnitConverter.cpp` | 运动执行链内部协同阶段 | 当运动控制内部协作逻辑需要被调用时触发。 | 承担该环节中的主要行为实现。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/adapters/device/src/legacy/adapters/motion/controller/interpolation/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `InterpolationAdapter.cpp` | 执行时的插补指令下发阶段 | 当轨迹需要被拆分为底层插补控制调用时触发。 | 把轨迹细化成控制周期可消费的插补数据。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/adapters/device/src/legacy/adapters/motion/controller/runtime/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `MotionRuntimeConnectionAdapter.cpp` | 执行中的运动运行时状态管理阶段 | 当执行过程中需要维护运动运行时状态时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `MotionRuntimeFacade.cpp` | 执行中的运动运行时状态管理阶段 | 当执行过程中需要维护运动运行时状态时触发。 | 维持执行过程中的会话、状态和资源协同。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/adapters/device/src/legacy/drivers/multicard/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `MultiCardAdapter.cpp` | 现场执行最底层的板卡驱动阶段 | 当更底层的板卡或驱动接口需要被调用时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `MultiCardStub_Class.cpp` | 现场执行最底层的板卡驱动阶段 | 当更底层的板卡或驱动接口需要被调用时触发。 | 承担该环节中的主要行为实现。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/adapters/device/src/motion/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `multicard_motion_device.cpp` | 现场执行时的运动设备适配阶段 | 当需要把运动动作映射到实际设备适配层时触发。 | 承担该环节中的主要行为实现。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/runtime/host/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `ContainerBootstrap.cpp` | 现场执行启动阶段 | 当运行时进程启动或准备创建一次执行会话时触发。 | 装配运行时依赖并建立点胶流程启动所需的执行环境。 | 输出运行时会话、调度结果或执行状态，供现场执行链继续推进。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/runtime/host/bootstrap/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `InfrastructureBindingsBuilder.cpp` | 现场执行启动阶段 | 当运行时进程启动或准备创建一次执行会话时触发。 | 承担该环节中的主要行为实现。 | 输出运行时会话、调度结果或执行状态，供现场执行链继续推进。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/runtime/host/container/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `ApplicationContainer.cpp` | 现场执行启动阶段 | 当运行时进程启动或准备创建一次执行会话时触发。 | 装配运行时依赖并建立点胶流程启动所需的执行环境。 | 输出运行时会话、调度结果或执行状态，供现场执行链继续推进。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `ApplicationContainer.Dispensing.cpp` | 现场执行启动阶段 | 当运行时进程启动或准备创建一次执行会话时触发。 | 装配运行时依赖并建立点胶流程启动所需的执行环境。 | 输出运行时会话、调度结果或执行状态，供现场执行链继续推进。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `ApplicationContainer.Motion.cpp` | 现场执行启动阶段 | 当运行时进程启动或准备创建一次执行会话时触发。 | 装配运行时依赖并建立点胶流程启动所需的执行环境。 | 输出运行时会话、调度结果或执行状态，供现场执行链继续推进。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `ApplicationContainer.Recipes.cpp` | 现场执行启动阶段 | 当运行时进程启动或准备创建一次执行会话时触发。 | 加载、校验或切换配方，使当前批次使用正确工艺参数。 | 输出运行时会话、调度结果或执行状态，供现场执行链继续推进。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `ApplicationContainer.System.cpp` | 现场执行启动阶段 | 当运行时进程启动或准备创建一次执行会话时触发。 | 装配运行时依赖并建立点胶流程启动所需的执行环境。 | 输出运行时会话、调度结果或执行状态，供现场执行链继续推进。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/runtime/host/factories/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `InfrastructureAdapterFactory.cpp` | 现场执行启动阶段 | 当运行时进程启动或准备创建一次执行会话时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出运行时会话、调度结果或执行状态，供现场执行链继续推进。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/runtime/host/runtime/configuration/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `ConfigFileAdapter.cpp` | 执行启动前的运行参数装载阶段 | 当执行会话启动前需要装载并校验运行参数时触发。 | 收敛设备、工艺或运行参数，为后续规划和执行提供边界条件。 | 输出经过装载或校验的运行配置，供执行会话启动使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `ConfigFileAdapter.Ini.cpp` | 执行启动前的运行参数装载阶段 | 当执行会话启动前需要装载并校验运行参数时触发。 | 收敛设备、工艺或运行参数，为后续规划和执行提供边界条件。 | 输出经过装载或校验的运行配置，供执行会话启动使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `ConfigFileAdapter.Sections.cpp` | 执行启动前的运行参数装载阶段 | 当执行会话启动前需要装载并校验运行参数时触发。 | 收敛设备、工艺或运行参数，为后续规划和执行提供边界条件。 | 输出经过装载或校验的运行配置，供执行会话启动使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `ConfigFileAdapter.System.cpp` | 执行启动前的运行参数装载阶段 | 当执行会话启动前需要装载并校验运行参数时触发。 | 收敛设备、工艺或运行参数，为后续规划和执行提供边界条件。 | 输出经过装载或校验的运行配置，供执行会话启动使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `InterlockConfigResolver.cpp` | 执行启动前的运行参数装载阶段 | 当执行会话启动前需要装载并校验运行参数时触发。 | 收敛设备、工艺或运行参数，为后续规划和执行提供边界条件。 | 输出经过装载或校验的运行配置，供执行会话启动使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/runtime/host/runtime/configuration/validators/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `ConfigValidator.cpp` | 执行启动前的配置校验阶段 | 当执行会话启动前需要装载并校验运行参数时触发。 | 收敛设备、工艺或运行参数，为后续规划和执行提供边界条件。 | 输出经过装载或校验的运行配置，供执行会话启动使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/runtime/host/runtime/diagnostics/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `DiagnosticsPortAdapter.cpp` | 现场执行过程中的事件与状态回收阶段 | 当现场执行过程中产生状态变化、事件或异常时触发。 | 记录执行过程中的关键事件和异常，支撑问题追溯。 | 输出运行事件、诊断记录或状态变化，供监控与追溯使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/runtime/host/runtime/events/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `InMemoryEventPublisherAdapter.cpp` | 现场执行过程中的事件与状态回收阶段 | 当现场执行过程中产生状态变化、事件或异常时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出运行事件、诊断记录或状态变化，供监控与追溯使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/runtime/host/runtime/recipes/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `AuditFileRepository.cpp` | 执行前的配方加载与切换阶段 | 当任务切换或执行前需要装载指定配方时触发。 | 承担该环节中的主要行为实现。 | 输出当前生效配方或配方加载结果，供执行链继续使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `ParameterSchemaFileProvider.cpp` | 执行前的配方加载与切换阶段 | 当任务切换或执行前需要装载指定配方时触发。 | 承担该环节中的主要行为实现。 | 输出当前生效配方或配方加载结果，供执行链继续使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `RecipeBundleSerializer.cpp` | 执行前的配方加载与切换阶段 | 当任务切换或执行前需要装载指定配方时触发。 | 加载、校验或切换配方，使当前批次使用正确工艺参数。 | 输出当前生效配方或配方加载结果，供执行链继续使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `RecipeFileRepository.cpp` | 执行前的配方加载与切换阶段 | 当任务切换或执行前需要装载指定配方时触发。 | 加载、校验或切换配方，使当前批次使用正确工艺参数。 | 输出当前生效配方或配方加载结果，供执行链继续使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `TemplateFileRepository.cpp` | 执行前的配方加载与切换阶段 | 当任务切换或执行前需要装载指定配方时触发。 | 承担该环节中的主要行为实现。 | 输出当前生效配方或配方加载结果，供执行链继续使用。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/runtime/host/runtime/scheduling/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `TaskSchedulerAdapter.cpp` | 现场执行中的任务调度阶段 | 当执行会话需要安排动作顺序或调度任务时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出运行时会话、调度结果或执行状态，供现场执行链继续推进。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/runtime/host/runtime/storage/files/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `LocalFileStorageAdapter.cpp` | 执行过程中的结果落盘阶段 | 当执行结果、配置或运行产物需要落盘时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出落盘文件、持久化结果或运行产物。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**apps/runtime-service/security/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `AuditLogger.cpp` | 执行入口的安全控制阶段 | 当执行入口涉及权限、策略或敏感配置保护时触发。 | 承担该环节中的主要行为实现。 | 输出运行时会话、调度结果或执行状态，供现场执行链继续推进。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `InterlockMonitor.cpp` | 执行入口的安全控制阶段 | 当执行入口涉及权限、策略或敏感配置保护时触发。 | 执行安全联锁或限位判断，防止设备在危险条件下继续动作。 | 输出运行时会话、调度结果或执行状态，供现场执行链继续推进。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `NetworkAccessControl.cpp` | 执行入口的安全控制阶段 | 当执行入口涉及权限、策略或敏感配置保护时触发。 | 承担该环节中的主要行为实现。 | 输出运行时会话、调度结果或执行状态，供现场执行链继续推进。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `NetworkWhitelist.cpp` | 执行入口的安全控制阶段 | 当执行入口涉及权限、策略或敏感配置保护时触发。 | 承担该环节中的主要行为实现。 | 输出运行时会话、调度结果或执行状态，供现场执行链继续推进。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `PasswordHasher.cpp` | 执行入口的安全控制阶段 | 当执行入口涉及权限、策略或敏感配置保护时触发。 | 承担该环节中的主要行为实现。 | 输出运行时会话、调度结果或执行状态，供现场执行链继续推进。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `PermissionService.cpp` | 执行入口的安全控制阶段 | 当执行入口涉及权限、策略或敏感配置保护时触发。 | 承担该环节中的主要行为实现。 | 输出运行时会话、调度结果或执行状态，供现场执行链继续推进。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `SafetyLimitsValidator.cpp` | 执行入口的安全控制阶段 | 当执行入口涉及权限、策略或敏感配置保护时触发。 | 执行安全联锁或限位判断，防止设备在危险条件下继续动作。 | 输出运行时会话、调度结果或执行状态，供现场执行链继续推进。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `SecurityService.cpp` | 执行入口的安全控制阶段 | 当执行入口涉及权限、策略或敏感配置保护时触发。 | 承担该环节中的主要行为实现。 | 输出运行时会话、调度结果或执行状态，供现场执行链继续推进。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `SessionService.cpp` | 执行入口的安全控制阶段 | 当执行入口涉及权限、策略或敏感配置保护时触发。 | 承担该环节中的主要行为实现。 | 输出运行时会话、调度结果或执行状态，供现场执行链继续推进。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `UserService.cpp` | 执行入口的安全控制阶段 | 当执行入口涉及权限、策略或敏感配置保护时触发。 | 承担该环节中的主要行为实现。 | 输出运行时会话、调度结果或执行状态，供现场执行链继续推进。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**apps/runtime-service/security/config/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `ConfigurationVersionService.cpp` | 执行入口的安全控制阶段 | 当执行入口涉及权限、策略或敏感配置保护时触发。 | 收敛设备、工艺或运行参数，为后续规划和执行提供边界条件。 | 输出运行时会话、调度结果或执行状态，供现场执行链继续推进。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `SecurityConfigLoader.cpp` | 执行入口的安全控制阶段 | 当执行入口涉及权限、策略或敏感配置保护时触发。 | 收敛设备、工艺或运行参数，为后续规划和执行提供边界条件。 | 输出运行时会话、调度结果或执行状态，供现场执行链继续推进。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/runtime/host/services/motion/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `HardLimitMonitorService.cpp` | 执行链中的运动服务支撑阶段 | 当需要把运动动作映射到实际设备适配层时触发。 | 承担该环节中的主要行为实现。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `SoftLimitMonitorService.cpp` | 执行链中的运动服务支撑阶段 | 当需要把运动动作映射到实际设备适配层时触发。 | 承担该环节中的主要行为实现。 | 输出对现场设备可直接执行的驱动调用、IO 指令、运动指令或阀控动作。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/runtime/host/tests/unit/runtime/configuration/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `ConfigFileAdapterHardwareConfigurationTest.cpp` | 执行启动前的运行参数装载阶段 | 当执行会话启动前需要装载并校验运行参数时触发。 | 收敛设备、工艺或运行参数，为后续规划和执行提供边界条件。 | 输出测试验证结果，用于证明执行链行为是否符合预期。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `InterlockConfigResolverTest.cpp` | 执行启动前的运行参数装载阶段 | 当执行会话启动前需要装载并校验运行参数时触发。 | 收敛设备、工艺或运行参数，为后续规划和执行提供边界条件。 | 输出测试验证结果，用于证明执行链行为是否符合预期。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |
| `WorkspaceAssetPathsTest.cpp` | 执行启动前的运行参数装载阶段 | 当执行会话启动前需要装载并校验运行参数时触发。 | 验证对应流程环节在正常和异常条件下是否符合预期。 | 输出测试验证结果，用于证明执行链行为是否符合预期。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/runtime/host/tests/unit/runtime/motion/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `MotionRuntimeFacadeTest.cpp` | 现场执行时的运动设备适配阶段 | 当需要把运动动作映射到实际设备适配层时触发。 | 维持执行过程中的会话、状态和资源协同。 | 输出测试验证结果，用于证明执行链行为是否符合预期。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

**runtime-execution/runtime/host/tests/unit/runtime/recipes/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `TemplateFileRepositoryTest.cpp` | 执行前的配方加载与切换阶段 | 当任务切换或执行前需要装载指定配方时触发。 | 验证对应流程环节在正常和异常条件下是否符合预期。 | 输出测试验证结果，用于证明执行链行为是否符合预期。 | 如果这里出错，问题会直接落到现场设备，表现为无法启动、走位异常、阀控异常或状态回收失败。 |

## topology-feature

- 模块编号：`M3`
- Owner 对象：`TopologyModel`
- 所属分组：`upstream`

**文件树**
```text
topology-feature/
├── domain/
│   └── geometry/
│       ├── CMakeLists.txt
│       ├── ContourAugmenterAdapter.cpp
│       ├── ContourAugmenterAdapter.h
│       └── ContourAugmenterAdapter.stub.cpp
├── CMakeLists.txt
└── module.yaml
```

**实现文件说明表**

**topology-feature/domain/geometry/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `ContourAugmenterAdapter.cpp` | 几何输入之后的拓扑特征提炼阶段 | 当几何图形已完成基础解析，需要进一步识别轮廓关系和特征时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出拓扑关系、轮廓增强或特征结果，供工艺规划继续消费。 | 如果这里出错，工艺规划会基于错误特征做判断，进而影响后续整条链路。 |
| `ContourAugmenterAdapter.stub.cpp` | 几何输入之后的拓扑特征提炼阶段 | 当几何图形已完成基础解析，需要进一步识别轮廓关系和特征时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出拓扑关系、轮廓增强或特征结果，供工艺规划继续消费。 | 如果这里出错，工艺规划会基于错误特征做判断，进而影响后续整条链路。 |

## trace-diagnostics

- 模块编号：`M10`
- Owner 对象：`ExecutionRecord`
- 所属分组：`runtime`
- 模块职责：追溯事件、诊断日志与审计线索的归档语义。；面向 HMI、验证与运维读取场景的追溯查询口径。；汇聚 `M10` owner 专属 contracts 与日志边界。

**文件树**
```text
trace-diagnostics/
├── adapters/
│   └── logging/
│       └── spdlog/
│           ├── sinks/
│           │   └── MemorySink.h
│           ├── SpdlogLoggingAdapter.cpp
│           └── SpdlogLoggingAdapter.h
├── CMakeLists.txt
└── module.yaml
```

**实现文件说明表**

**trace-diagnostics/adapters/logging/spdlog/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `SpdlogLoggingAdapter.cpp` | 执行过程中的日志采集与诊断记录阶段 | 当运行过程产生日志、事件或异常需要被记录时触发。 | 记录执行过程中的关键事件和异常，支撑问题追溯。 | 输出结构化日志或诊断记录，供 HMI、运维和问题分析使用。 | 如果这里出错，现场问题将难以及时定位和回放。 |

## workflow

- 模块编号：`M0`
- Owner 对象：`WorkflowRun`
- 所属分组：`workflow`
- 模块职责：工作流编排与阶段推进语义；规划链（`M4-M8`）触发与编排边界；流程状态与调度决策的事实归属

**文件树**
```text
workflow/
├── adapters/
│   ├── include/
│   │   ├── infrastructure/
│   │   │   └── adapters/
│   │   │       └── planning/
│   │   │           ├── dxf/
│   │   │           │   ├── AutoPathSourceAdapter.h
│   │   │           │   ├── DXFAdapterFactory.h
│   │   │           │   └── PbPathSourceAdapter.h
│   │   │           └── geometry/
│   │   │               └── ContourAugmenterAdapter.h
│   │   └── recipes/
│   │       └── serialization/
│   │           └── RecipeJsonSerializer.h
│   ├── infrastructure/
│   │   └── adapters/
│   │       ├── planning/
│   │       │   ├── dxf/
│   │       │   │   ├── AutoPathSourceAdapter.cpp
│   │       │   │   ├── AutoPathSourceAdapter.h
│   │       │   │   ├── CMakeLists.txt
│   │       │   │   ├── DXFAdapterFactory.cpp
│   │       │   │   ├── DXFAdapterFactory.h
│   │       │   │   ├── PbPathSourceAdapter.cpp
│   │       │   │   └── PbPathSourceAdapter.h
│   │       │   └── geometry/
│   │       │       ├── CMakeLists.txt
│   │       │       ├── ContourAugmenterAdapter.cpp
│   │       │       ├── ContourAugmenterAdapter.h
│   │       │       └── ContourAugmenterAdapter.stub.cpp
│   │       └── redundancy/
│   │           ├── CMakeLists.txt
│   │           ├── JsonRedundancyRepositoryAdapter.cpp
│   │           └── JsonRedundancyRepositoryAdapter.h
│   ├── recipes/
│   │   └── serialization/
│   │       ├── CMakeLists.txt
│   │       ├── RecipeJsonSerializer.cpp
│   │       └── RecipeJsonSerializer.h
│   └── CMakeLists.txt
├── application/
│   ├── include/
│   │   └── application/
│   │       └── usecases/
│   │           ├── dispensing/
│   │           │   ├── valve/
│   │           │   │   ├── ValveCommandUseCase.h
│   │           │   │   └── ValveQueryUseCase.h
│   │           │   ├── CleanupFilesUseCase.h
│   │           │   ├── DispensingExecutionUseCase.h
│   │           │   ├── DispensingWorkflowUseCase.h
│   │           │   ├── PlanningUseCase.h
│   │           │   └── UploadFileUseCase.h
│   │           ├── motion/
│   │           │   ├── coordination/
│   │           │   │   └── MotionCoordinationUseCase.h
│   │           │   ├── homing/
│   │           │   │   └── HomeAxesUseCase.h
│   │           │   ├── initialization/
│   │           │   │   └── MotionInitializationUseCase.h
│   │           │   ├── interpolation/
│   │           │   │   └── InterpolationPlanningUseCase.h
│   │           │   ├── manual/
│   │           │   │   └── ManualMotionControlUseCase.h
│   │           │   ├── monitoring/
│   │           │   │   └── MotionMonitoringUseCase.h
│   │           │   ├── safety/
│   │           │   │   └── MotionSafetyUseCase.h
│   │           │   └── trajectory/
│   │           │       └── ExecuteTrajectoryUseCase.h
│   │           ├── recipes/
│   │           │   ├── CompareRecipeVersionsUseCase.h
│   │           │   ├── CreateDraftVersionUseCase.h
│   │           │   ├── CreateRecipeUseCase.h
│   │           │   ├── CreateVersionFromPublishedUseCase.h
│   │           │   ├── ExportRecipeBundlePayloadUseCase.h
│   │           │   ├── ImportRecipeBundlePayloadUseCase.h
│   │           │   ├── RecipeCommandUseCase.h
│   │           │   ├── RecipeQueryUseCase.h
│   │           │   ├── UpdateDraftVersionUseCase.h
│   │           │   └── UpdateRecipeUseCase.h
│   │           └── system/
│   │               ├── EmergencyStopUseCase.h
│   │               ├── IHardLimitMonitor.h
│   │               └── InitializeSystemUseCase.h
│   ├── services/
│   │   ├── dxf/
│   │   │   ├── DxfPbPreparationService.cpp
│   │   │   └── DxfPbPreparationService.h
│   │   └── redundancy/
│   │       ├── CandidateScoringService.cpp
│   │       └── CandidateScoringService.h
│   ├── usecases/
│   │   ├── dispensing/
│   │   │   ├── valve/
│   │   │   │   ├── coordination/
│   │   │   │   │   ├── ValveCoordinationUseCase.cpp
│   │   │   │   │   └── ValveCoordinationUseCase.h
│   │   │   │   ├── CMakeLists.txt
│   │   │   │   ├── ValveCommandUseCase.cpp
│   │   │   │   ├── ValveCommandUseCase.h
│   │   │   │   ├── ValveQueryUseCase.cpp
│   │   │   │   ├── ValveQueryUseCase.h
│   │   │   │   └── ValveUseCasePreconditions.h
│   │   │   ├── CleanupFilesUseCase.cpp
│   │   │   ├── CleanupFilesUseCase.h
│   │   │   ├── DispensingExecutionUseCase.Async.cpp
│   │   │   ├── DispensingExecutionUseCase.Control.cpp
│   │   │   ├── DispensingExecutionUseCase.cpp
│   │   │   ├── DispensingExecutionUseCase.h
│   │   │   ├── DispensingExecutionUseCase.Setup.cpp
│   │   │   ├── DispensingWorkflowUseCase.cpp
│   │   │   ├── DispensingWorkflowUseCase.h
│   │   │   ├── PlanningUseCase.cpp
│   │   │   ├── PlanningUseCase.h
│   │   │   ├── UploadFileUseCase.cpp
│   │   │   └── UploadFileUseCase.h
│   │   ├── motion/
│   │   │   ├── coordination/
│   │   │   │   ├── MotionCoordinationUseCase.cpp
│   │   │   │   └── MotionCoordinationUseCase.h
│   │   │   ├── homing/
│   │   │   │   ├── HomeAxesUseCase.cpp
│   │   │   │   └── HomeAxesUseCase.h
│   │   │   ├── initialization/
│   │   │   │   ├── MotionInitializationUseCase.cpp
│   │   │   │   └── MotionInitializationUseCase.h
│   │   │   ├── interpolation/
│   │   │   │   ├── InterpolationPlanningUseCase.cpp
│   │   │   │   └── InterpolationPlanningUseCase.h
│   │   │   ├── manual/
│   │   │   │   ├── ManualMotionControlUseCase.cpp
│   │   │   │   └── ManualMotionControlUseCase.h
│   │   │   ├── monitoring/
│   │   │   │   ├── MotionMonitoringUseCase.cpp
│   │   │   │   └── MotionMonitoringUseCase.h
│   │   │   ├── ptp/
│   │   │   │   ├── MoveToPositionUseCase.cpp
│   │   │   │   └── MoveToPositionUseCase.h
│   │   │   ├── runtime/
│   │   │   │   ├── MotionRuntimeAssemblyFactory.cpp
│   │   │   │   └── MotionRuntimeAssemblyFactory.h
│   │   │   ├── safety/
│   │   │   │   ├── MotionSafetyUseCase.cpp
│   │   │   │   └── MotionSafetyUseCase.h
│   │   │   └── trajectory/
│   │   │       ├── DeterministicPathExecutionUseCase.cpp
│   │   │       ├── DeterministicPathExecutionUseCase.h
│   │   │       ├── ExecuteTrajectoryUseCase.cpp
│   │   │       └── ExecuteTrajectoryUseCase.h
│   │   ├── recipes/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── CompareRecipeVersionsUseCase.cpp
│   │   │   ├── CompareRecipeVersionsUseCase.h
│   │   │   ├── CreateDraftVersionUseCase.cpp
│   │   │   ├── CreateDraftVersionUseCase.h
│   │   │   ├── CreateRecipeUseCase.cpp
│   │   │   ├── CreateRecipeUseCase.h
│   │   │   ├── CreateVersionFromPublishedUseCase.cpp
│   │   │   ├── CreateVersionFromPublishedUseCase.h
│   │   │   ├── ExportRecipeBundlePayloadUseCase.cpp
│   │   │   ├── ExportRecipeBundlePayloadUseCase.h
│   │   │   ├── ImportRecipeBundlePayloadUseCase.cpp
│   │   │   ├── ImportRecipeBundlePayloadUseCase.h
│   │   │   ├── RecipeCommandUseCase.cpp
│   │   │   ├── RecipeCommandUseCase.h
│   │   │   ├── RecipeQueryUseCase.cpp
│   │   │   ├── RecipeQueryUseCase.h
│   │   │   ├── RecipeUseCaseHelpers.h
│   │   │   ├── RecipeUsecaseModule.h
│   │   │   ├── UpdateDraftVersionUseCase.cpp
│   │   │   ├── UpdateDraftVersionUseCase.h
│   │   │   ├── UpdateRecipeUseCase.cpp
│   │   │   └── UpdateRecipeUseCase.h
│   │   ├── redundancy/
│   │   │   ├── RedundancyGovernanceUseCases.cpp
│   │   │   └── RedundancyGovernanceUseCases.h
│   │   └── system/
│   │       ├── EmergencyStopUseCase.cpp
│   │       ├── EmergencyStopUseCase.h
│   │       ├── IHardLimitMonitor.h
│   │       ├── InitializeSystemUseCase.cpp
│   │       └── InitializeSystemUseCase.h
│   ├── usecases-bridge/
│   │   └── CMakeLists.txt
│   └── CMakeLists.txt
├── domain/
│   ├── domain/
│   │   ├── _shared/
│   │   │   └── CMakeLists.txt
│   │   ├── configuration/
│   │   │   ├── ports/
│   │   │   │   ├── IConfigurationPort.h
│   │   │   │   ├── IFileStoragePort.h
│   │   │   │   └── ValveConfig.h
│   │   │   ├── value-objects/
│   │   │   │   └── ConfigTypes.h
│   │   │   ├── CMakeLists.txt
│   │   │   ├── CMPPulseConfig.cpp
│   │   │   ├── DispensingConfig.cpp
│   │   │   ├── InterpolationConfig.cpp
│   │   │   ├── MachineConfig.cpp
│   │   │   └── ValveTimingConfig.cpp
│   │   ├── diagnostics/
│   │   │   ├── aggregates/
│   │   │   │   ├── TestConfiguration.h
│   │   │   │   └── TestRecord.h
│   │   │   ├── domain-services/
│   │   │   │   ├── ProcessResultSerialization.cpp
│   │   │   │   ├── ProcessResultSerialization.h
│   │   │   │   ├── ProcessResultService.cpp
│   │   │   │   └── ProcessResultService.h
│   │   │   ├── ports/
│   │   │   │   ├── ICMPTestPresetPort.h
│   │   │   │   ├── IDiagnosticsPort.h
│   │   │   │   ├── ITestConfigurationPort.h
│   │   │   │   └── ITestRecordRepository.h
│   │   │   ├── value-objects/
│   │   │   │   ├── BugReportTypes.h
│   │   │   │   ├── DiagnosticTypes.h
│   │   │   │   └── TestDataTypes.h
│   │   │   └── CMakeLists.txt
│   │   ├── dispensing/
│   │   │   ├── compensation/
│   │   │   ├── domain-services/
│   │   │   │   ├── ArcTriggerPointCalculator.cpp
│   │   │   │   ├── ArcTriggerPointCalculator.h
│   │   │   │   ├── CMPTriggerService.cpp
│   │   │   │   ├── CMPTriggerService.h
│   │   │   │   ├── DispenseCompensationService.cpp
│   │   │   │   ├── DispenseCompensationService.h
│   │   │   │   ├── DispensingController.cpp
│   │   │   │   ├── DispensingController.h
│   │   │   │   ├── PathOptimizationStrategy.cpp
│   │   │   │   ├── PathOptimizationStrategy.h
│   │   │   │   ├── PurgeDispenserProcess.cpp
│   │   │   │   ├── PurgeDispenserProcess.h
│   │   │   │   ├── SupplyStabilizationPolicy.cpp
│   │   │   │   ├── SupplyStabilizationPolicy.h
│   │   │   │   ├── TriggerPlanner.cpp
│   │   │   │   ├── TriggerPlanner.h
│   │   │   │   ├── ValveCoordinationService.cpp
│   │   │   │   └── ValveCoordinationService.h
│   │   │   ├── model/
│   │   │   ├── planning/
│   │   │   │   └── domain-services/
│   │   │   │       ├── ContourOptimizationService.cpp
│   │   │   │       ├── ContourOptimizationService.h
│   │   │   │       ├── DispensingPlannerService.cpp
│   │   │   │       ├── DispensingPlannerService.h
│   │   │   │       ├── UnifiedTrajectoryPlannerService.cpp
│   │   │   │       └── UnifiedTrajectoryPlannerService.h
│   │   │   ├── ports/
│   │   │   │   ├── IDispensingExecutionObserver.h
│   │   │   │   ├── ITaskSchedulerPort.h
│   │   │   │   ├── ITriggerControllerPort.h
│   │   │   │   └── IValvePort.h
│   │   │   ├── simulation/
│   │   │   ├── value-objects/
│   │   │   │   ├── DispenseCompensationProfile.h
│   │   │   │   ├── DispensingExecutionTypes.h
│   │   │   │   ├── DispensingPlan.h
│   │   │   │   ├── QualityMetrics.h
│   │   │   │   ├── SafetyBoundary.h
│   │   │   │   └── TriggerPlan.h
│   │   │   └── CMakeLists.txt
│   │   ├── geometry/
│   │   │   └── GeometryTypes.h
│   │   ├── machine/
│   │   │   ├── aggregates/
│   │   │   │   ├── DispenserModel.cpp
│   │   │   │   └── DispenserModel.h
│   │   │   ├── domain-services/
│   │   │   │   ├── CalibrationProcess.cpp
│   │   │   │   └── CalibrationProcess.h
│   │   │   ├── ports/
│   │   │   │   ├── ICalibrationDevicePort.h
│   │   │   │   ├── ICalibrationResultPort.h
│   │   │   │   ├── IHardwareConnectionPort.h
│   │   │   │   └── IHardwareTestPort.h
│   │   │   ├── value-objects/
│   │   │   │   └── CalibrationTypes.h
│   │   │   └── CMakeLists.txt
│   │   ├── motion/
│   │   │   ├── domain-services/
│   │   │   │   ├── interpolation/
│   │   │   │   │   ├── ArcGeometryMath.cpp
│   │   │   │   │   ├── ArcGeometryMath.h
│   │   │   │   │   ├── ArcInterpolator.cpp
│   │   │   │   │   ├── ArcInterpolator.h
│   │   │   │   │   ├── InterpolationCommandValidator.cpp
│   │   │   │   │   ├── InterpolationCommandValidator.h
│   │   │   │   │   ├── InterpolationProgramPlanner.cpp
│   │   │   │   │   ├── InterpolationProgramPlanner.h
│   │   │   │   │   ├── LinearInterpolator.cpp
│   │   │   │   │   ├── LinearInterpolator.h
│   │   │   │   │   ├── SplineGeometryMath.cpp
│   │   │   │   │   ├── SplineGeometryMath.h
│   │   │   │   │   ├── SplineInterpolator.cpp
│   │   │   │   │   ├── SplineInterpolator.h
│   │   │   │   │   ├── TrajectoryInterpolatorBase.cpp
│   │   │   │   │   ├── TrajectoryInterpolatorBase.h
│   │   │   │   │   ├── ValidatedInterpolationPort.cpp
│   │   │   │   │   └── ValidatedInterpolationPort.h
│   │   │   │   ├── GeometryBlender.cpp
│   │   │   │   ├── GeometryBlender.h
│   │   │   │   ├── HomingProcess.cpp
│   │   │   │   ├── HomingProcess.h
│   │   │   │   ├── JogController.cpp
│   │   │   │   ├── JogController.h
│   │   │   │   ├── MotionBufferController.cpp
│   │   │   │   ├── MotionBufferController.h
│   │   │   │   ├── MotionControlService.h
│   │   │   │   ├── MotionControlServiceImpl.cpp
│   │   │   │   ├── MotionControlServiceImpl.h
│   │   │   │   ├── MotionStatusService.h
│   │   │   │   ├── MotionStatusServiceImpl.cpp
│   │   │   │   ├── MotionStatusServiceImpl.h
│   │   │   │   ├── MotionValidationService.h
│   │   │   │   ├── SevenSegmentSCurveProfile.cpp
│   │   │   │   ├── SevenSegmentSCurveProfile.h
│   │   │   │   ├── SpeedPlanner.cpp
│   │   │   │   ├── SpeedPlanner.h
│   │   │   │   ├── TimeTrajectoryPlanner.cpp
│   │   │   │   ├── TimeTrajectoryPlanner.h
│   │   │   │   ├── TrajectoryPlanner.cpp
│   │   │   │   ├── TrajectoryPlanner.h
│   │   │   │   ├── TriggerCalculator.cpp
│   │   │   │   ├── TriggerCalculator.h
│   │   │   │   ├── VelocityProfileService.cpp
│   │   │   │   └── VelocityProfileService.h
│   │   │   ├── ports/
│   │   │   │   ├── IAdvancedMotionPort.h
│   │   │   │   ├── IAxisControlPort.h
│   │   │   │   ├── IHomingPort.h
│   │   │   │   ├── IInterpolationPort.h
│   │   │   │   ├── IIOControlPort.h
│   │   │   │   ├── IJogControlPort.h
│   │   │   │   ├── IMotionConnectionPort.h
│   │   │   │   ├── IMotionRuntimePort.h
│   │   │   │   ├── IMotionStatePort.h
│   │   │   │   ├── IPositionControlPort.h
│   │   │   │   └── IVelocityProfilePort.h
│   │   │   ├── value-objects/
│   │   │   │   ├── HardwareTestTypes.h
│   │   │   │   ├── MotionTrajectory.h
│   │   │   │   ├── MotionTypes.h
│   │   │   │   ├── SemanticPath.h
│   │   │   │   ├── TimePlanningConfig.h
│   │   │   │   ├── TrajectoryAnalysisTypes.h
│   │   │   │   └── TrajectoryTypes.h
│   │   │   ├── BezierCalculator.cpp
│   │   │   ├── BezierCalculator.h
│   │   │   ├── BSplineCalculator.cpp
│   │   │   ├── BSplineCalculator.h
│   │   │   ├── CircleCalculator.cpp
│   │   │   ├── CircleCalculator.h
│   │   │   ├── CMakeLists.txt
│   │   │   ├── CMPCompensation.cpp
│   │   │   ├── CMPCompensation.h
│   │   │   ├── CMPCoordinatedInterpolator.cpp
│   │   │   ├── CMPCoordinatedInterpolator.h
│   │   │   ├── CMPValidator.cpp
│   │   │   └── CMPValidator.h
│   │   ├── planning/
│   │   │   ├── ports/
│   │   │   │   └── ISpatialIndexPort.h
│   │   │   └── CMakeLists.txt
│   │   ├── recipes/
│   │   │   ├── aggregates/
│   │   │   │   ├── Recipe.h
│   │   │   │   └── RecipeVersion.h
│   │   │   ├── domain-services/
│   │   │   │   ├── RecipeActivationService.cpp
│   │   │   │   ├── RecipeActivationService.h
│   │   │   │   ├── RecipeValidationService.cpp
│   │   │   │   └── RecipeValidationService.h
│   │   │   ├── ports/
│   │   │   │   ├── IAuditRepositoryPort.h
│   │   │   │   ├── IParameterSchemaPort.h
│   │   │   │   ├── IRecipeBundleSerializerPort.h
│   │   │   │   ├── IRecipeRepositoryPort.h
│   │   │   │   └── ITemplateRepositoryPort.h
│   │   │   ├── value-objects/
│   │   │   │   ├── ParameterSchema.h
│   │   │   │   ├── RecipeBundle.h
│   │   │   │   └── RecipeTypes.h
│   │   │   ├── CMakeLists.txt
│   │   │   └── RecipeModule.h
│   │   ├── safety/
│   │   │   ├── domain-services/
│   │   │   │   ├── EmergencyStopService.cpp
│   │   │   │   ├── EmergencyStopService.h
│   │   │   │   ├── InterlockPolicy.cpp
│   │   │   │   ├── InterlockPolicy.h
│   │   │   │   ├── SoftLimitValidator.cpp
│   │   │   │   └── SoftLimitValidator.h
│   │   │   ├── ports/
│   │   │   │   └── IInterlockSignalPort.h
│   │   │   ├── value-objects/
│   │   │   │   └── InterlockTypes.h
│   │   │   └── CMakeLists.txt
│   │   ├── system/
│   │   │   ├── ports/
│   │   │   │   └── IEventPublisherPort.h
│   │   │   ├── redundancy/
│   │   │   │   ├── ports/
│   │   │   │   │   └── IRedundancyRepositoryPort.h
│   │   │   │   └── RedundancyTypes.h
│   │   │   └── CMakeLists.txt
│   │   ├── trajectory/
│   │   │   ├── domain-services/
│   │   │   │   ├── GeometryNormalizer.cpp
│   │   │   │   ├── GeometryNormalizer.h
│   │   │   │   ├── MotionPlanner.cpp
│   │   │   │   ├── MotionPlanner.h
│   │   │   │   ├── PathRegularizer.cpp
│   │   │   │   ├── PathRegularizer.h
│   │   │   │   ├── ProcessAnnotator.cpp
│   │   │   │   ├── ProcessAnnotator.h
│   │   │   │   ├── SplineApproximation.cpp
│   │   │   │   ├── SplineApproximation.h
│   │   │   │   ├── TrajectoryShaper.cpp
│   │   │   │   └── TrajectoryShaper.h
│   │   │   ├── ports/
│   │   │   │   ├── IDXFPathSourcePort.h
│   │   │   │   └── IPathSourcePort.h
│   │   │   ├── value-objects/
│   │   │   │   ├── GeometryBoostAdapter.h
│   │   │   │   ├── GeometryUtils.h
│   │   │   │   ├── MotionConfig.h
│   │   │   │   ├── Path.h
│   │   │   │   ├── PlanningReport.h
│   │   │   │   ├── Primitive.h
│   │   │   │   ├── ProcessConfig.h
│   │   │   │   ├── ProcessPath.h
│   │   │   │   └── Trajectory.h
│   │   │   └── CMakeLists.txt
│   │   └── CMakeLists.txt
│   ├── include/
│   │   └── domain/
│   │       ├── configuration/
│   │       │   └── ports/
│   │       │       ├── IConfigurationPort.h
│   │       │       ├── IFileStoragePort.h
│   │       │       └── ValveConfig.h
│   │       ├── diagnostics/
│   │       │   ├── aggregates/
│   │       │   │   ├── TestConfiguration.h
│   │       │   │   └── TestRecord.h
│   │       │   ├── ports/
│   │       │   │   ├── ICMPTestPresetPort.h
│   │       │   │   ├── IDiagnosticsPort.h
│   │       │   │   ├── ITestConfigurationPort.h
│   │       │   │   └── ITestRecordRepository.h
│   │       │   └── value-objects/
│   │       │       ├── DiagnosticTypes.h
│   │       │       └── TestDataTypes.h
│   │       ├── dispensing/
│   │       │   ├── domain-services/
│   │       │   │   ├── CMPTriggerService.h
│   │       │   │   ├── DispenseCompensationService.h
│   │       │   │   ├── PurgeDispenserProcess.h
│   │       │   │   ├── SupplyStabilizationPolicy.h
│   │       │   │   └── ValveCoordinationService.h
│   │       │   ├── planning/
│   │       │   │   └── domain-services/
│   │       │   │       └── DispensingPlannerService.h
│   │       │   ├── ports/
│   │       │   │   ├── ITaskSchedulerPort.h
│   │       │   │   ├── ITriggerControllerPort.h
│   │       │   │   └── IValvePort.h
│   │       │   └── value-objects/
│   │       │       ├── DispenseCompensationProfile.h
│   │       │       ├── DispensingExecutionTypes.h
│   │       │       ├── QualityMetrics.h
│   │       │       └── SafetyBoundary.h
│   │       ├── machine/
│   │       │   ├── aggregates/
│   │       │   │   └── DispenserModel.h
│   │       │   └── ports/
│   │       │       ├── IHardwareConnectionPort.h
│   │       │       └── IHardwareTestPort.h
│   │       ├── motion/
│   │       │   ├── domain-services/
│   │       │   │   ├── interpolation/
│   │       │   │   │   ├── InterpolationCommandValidator.h
│   │       │   │   │   ├── TrajectoryInterpolatorBase.h
│   │       │   │   │   └── ValidatedInterpolationPort.h
│   │       │   │   ├── HomingProcess.h
│   │       │   │   ├── JogController.h
│   │       │   │   ├── MotionControlService.h
│   │       │   │   ├── MotionControlServiceImpl.h
│   │       │   │   ├── MotionStatusService.h
│   │       │   │   ├── MotionStatusServiceImpl.h
│   │       │   │   ├── SevenSegmentSCurveProfile.h
│   │       │   │   └── VelocityProfileService.h
│   │       │   ├── ports/
│   │       │   │   ├── IAdvancedMotionPort.h
│   │       │   │   ├── IAxisControlPort.h
│   │       │   │   ├── IHomingPort.h
│   │       │   │   ├── IInterpolationPort.h
│   │       │   │   ├── IIOControlPort.h
│   │       │   │   ├── IJogControlPort.h
│   │       │   │   ├── IMotionConnectionPort.h
│   │       │   │   ├── IMotionRuntimePort.h
│   │       │   │   ├── IMotionStatePort.h
│   │       │   │   ├── IPositionControlPort.h
│   │       │   │   └── IVelocityProfilePort.h
│   │       │   └── value-objects/
│   │       │       ├── HardwareTestTypes.h
│   │       │       ├── MotionTrajectory.h
│   │       │       ├── MotionTypes.h
│   │       │       ├── TrajectoryAnalysisTypes.h
│   │       │       └── TrajectoryTypes.h
│   │       ├── planning/
│   │       │   └── ports/
│   │       │       └── ISpatialIndexPort.h
│   │       ├── recipes/
│   │       │   ├── aggregates/
│   │       │   │   ├── Recipe.h
│   │       │   │   └── RecipeVersion.h
│   │       │   ├── ports/
│   │       │   │   ├── IAuditRepositoryPort.h
│   │       │   │   ├── IParameterSchemaPort.h
│   │       │   │   ├── IRecipeBundleSerializerPort.h
│   │       │   │   ├── IRecipeRepositoryPort.h
│   │       │   │   └── ITemplateRepositoryPort.h
│   │       │   └── value-objects/
│   │       │       ├── ParameterSchema.h
│   │       │       ├── RecipeBundle.h
│   │       │       └── RecipeTypes.h
│   │       ├── safety/
│   │       │   ├── domain-services/
│   │       │   │   └── EmergencyStopService.h
│   │       │   ├── ports/
│   │       │   │   └── IInterlockSignalPort.h
│   │       │   └── value-objects/
│   │       │       └── InterlockTypes.h
│   │       ├── system/
│   │       │   └── ports/
│   │       │       └── IEventPublisherPort.h
│   │       └── trajectory/
│   │           ├── ports/
│   │           │   ├── IDXFPathSourcePort.h
│   │           │   └── IPathSourcePort.h
│   │           └── value-objects/
│   │               ├── Path.h
│   │               ├── PlanningReport.h
│   │               ├── Primitive.h
│   │               └── ProcessPath.h
│   ├── motion-core/
│   │   ├── include/
│   │   │   └── siligen/
│   │   │       └── motion/
│   │   │           ├── model/
│   │   │           │   └── motion_types.h
│   │   │           ├── ports/
│   │   │           │   ├── axis_feedback_port.h
│   │   │           │   ├── emergency_port.h
│   │   │           │   ├── interlock_port.h
│   │   │           │   └── motion_controller_port.h
│   │   │           ├── safety/
│   │   │           │   ├── ports/
│   │   │           │   │   └── interlock_signal_port.h
│   │   │           │   ├── services/
│   │   │           │   │   └── interlock_policy.h
│   │   │           │   └── interlock_types.h
│   │   │           └── services/
│   │   │               └── motion_service.h
│   │   ├── src/
│   │   │   └── safety/
│   │   │       └── services/
│   │   │           └── interlock_policy.cpp
│   │   └── CMakeLists.txt
│   ├── process-core/
│   │   ├── include/
│   │   │   └── siligen/
│   │   │       └── process/
│   │   │           ├── execution/
│   │   │           │   └── process_types.h
│   │   │           ├── ports/
│   │   │           │   ├── dispenser_actuator_port.h
│   │   │           │   ├── motion_executor_port.h
│   │   │           │   ├── recipe_repository_port.h
│   │   │           │   └── safety_guard_port.h
│   │   │           ├── recipes/
│   │   │           │   ├── aggregates/
│   │   │           │   │   ├── recipe.h
│   │   │           │   │   └── recipe_version.h
│   │   │           │   ├── ports/
│   │   │           │   │   ├── audit_repository_port.h
│   │   │           │   │   └── recipe_repository_port.h
│   │   │           │   ├── services/
│   │   │           │   │   ├── recipe_activation_service.h
│   │   │           │   │   └── recipe_validation_service.h
│   │   │           │   └── value_objects/
│   │   │           │       ├── parameter_schema.h
│   │   │           │       └── recipe_types.h
│   │   │           └── services/
│   │   │               └── process_execution_service.h
│   │   ├── src/
│   │   │   └── recipes/
│   │   │       └── services/
│   │   │           ├── recipe_activation_service.cpp
│   │   │           └── recipe_validation_service.cpp
│   │   └── CMakeLists.txt
│   └── CMakeLists.txt
├── process-runtime-core/
│   ├── docs/
│   │   └── redundancy/
│   │       ├── contracts/
│   │       │   └── redundancy-interface.contract.v1.json
│   │       └── model/
│   │           ├── redundancy-data-model.schema.v1.json
│   │           └── redundancy-enums.v1.json
│   ├── tests/
│   │   └── CMakeLists.txt
│   └── CMakeLists.txt
├── tests/
│   ├── process-runtime-core/
│   │   ├── unit/
│   │   │   ├── dispensing/
│   │   │   │   ├── CleanupFilesUseCaseTest.cpp
│   │   │   │   ├── ContourPathOptimizerTest.cpp
│   │   │   │   ├── DispensingExecutionUseCaseProgressTest.cpp
│   │   │   │   ├── DispensingWorkflowUseCaseTest.cpp
│   │   │   │   ├── PlanningRequestTest.cpp
│   │   │   │   └── UploadFileUseCaseTest.cpp
│   │   │   ├── domain/
│   │   │   │   ├── diagnostics/
│   │   │   │   │   └── ProcessResultServiceTest.cpp
│   │   │   │   ├── dispensing/
│   │   │   │   │   ├── ArcTriggerPointCalculatorTest.cpp
│   │   │   │   │   ├── ClosedLoopSkeletonTest.cpp
│   │   │   │   │   ├── DispensingControllerTest.cpp
│   │   │   │   │   ├── PurgeDispenserProcessTest.cpp
│   │   │   │   │   ├── TriggerPlannerTest.cpp
│   │   │   │   │   └── ValveCoordinationServiceTest.cpp
│   │   │   │   ├── machine/
│   │   │   │   │   └── CalibrationProcessTest.cpp
│   │   │   │   ├── motion/
│   │   │   │   │   ├── JogControllerTest.cpp
│   │   │   │   │   └── MotionBufferControllerTest.cpp
│   │   │   │   ├── recipes/
│   │   │   │   │   └── RecipeActivationServiceTest.cpp
│   │   │   │   ├── safety/
│   │   │   │   │   ├── EmergencyStopServiceTest.cpp
│   │   │   │   │   ├── InterlockPolicyTest.cpp
│   │   │   │   │   └── SoftLimitValidatorTest.cpp
│   │   │   │   └── trajectory/
│   │   │   │       ├── InterpolationProgramPlannerTest.cpp
│   │   │   │       ├── MotionPlannerConstraintTest.cpp
│   │   │   │       ├── MotionPlannerTest.cpp
│   │   │   │       └── PathRegularizerTest.cpp
│   │   │   ├── infrastructure/
│   │   │   │   └── adapters/
│   │   │   │       ├── planning/
│   │   │   │       │   └── dxf/
│   │   │   │       │       ├── DXFAdapterFactoryTest.cpp
│   │   │   │       │       └── PbPathSourceAdapterContractTest.cpp
│   │   │   │       └── redundancy/
│   │   │   │           └── JsonRedundancyRepositoryAdapterTest.cpp
│   │   │   ├── motion/
│   │   │   │   ├── DeterministicPathExecutionUseCaseTest.cpp
│   │   │   │   ├── HomeAxesUseCaseTest.cpp
│   │   │   │   ├── HomingSupportTest.cpp
│   │   │   │   ├── MotionCoordinationUseCaseTest.cpp
│   │   │   │   ├── MotionMonitoringUseCaseTest.cpp
│   │   │   │   ├── MotionRuntimeAssemblyFactoryTest.cpp
│   │   │   │   └── MotionSafetyUseCaseTest.cpp
│   │   │   ├── recipes/
│   │   │   │   └── RecipeCommandUseCaseTest.cpp
│   │   │   ├── redundancy/
│   │   │   │   ├── CandidateScoringServiceTest.cpp
│   │   │   │   └── RedundancyGovernanceUseCasesTest.cpp
│   │   │   └── system/
│   │   │       └── InitializeSystemUseCaseTest.cpp
│   │   └── CMakeLists.txt
│   └── CMakeLists.txt
├── CMakeLists.txt
└── module.yaml
```

**实现文件说明表**

**workflow/adapters/infrastructure/adapters/planning/dxf/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `AutoPathSourceAdapter.cpp` | 规划链中的 DXF 输入适配阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 对 canonical `.pb` 输入做统一入口校验，并对 direct `.dxf` 输入执行 hard-fail guard。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `DXFAdapterFactory.cpp` | 规划链中的 DXF 输入适配阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 组装 mock/local/remote DXF path source adapter，并把 local 行为收口到 canonical `.pb` 读取边界。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `PbPathSourceAdapter.cpp` | 规划链中的 DXF 输入适配阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 从 `.pb` 解析运行时路径 primitive，作为 live 规划链唯一正式输入。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/adapters/infrastructure/adapters/planning/geometry/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `ContourAugmenterAdapter.cpp` | 规划链中的几何增强适配阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `ContourAugmenterAdapter.stub.cpp` | 规划链中的几何增强适配阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/adapters/infrastructure/adapters/redundancy/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `JsonRedundancyRepositoryAdapter.cpp` | 冗余状态持久化适配阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 处理冗余状态或切换逻辑，保证异常情况下流程可恢复。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/adapters/recipes/serialization/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `RecipeJsonSerializer.cpp` | 配方持久化与交换阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 加载、校验或切换配方，使当前批次使用正确工艺参数。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/application/services/dxf/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `DxfPbPreparationService.cpp` | 流程编排中的工程输入服务阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/application/services/redundancy/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `CandidateScoringService.cpp` | 流程编排中的冗余支撑阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/application/usecases/dispensing/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `CleanupFilesUseCase.cpp` | 点胶业务编排阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `DispensingExecutionUseCase.Async.cpp` | 点胶业务编排阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `DispensingExecutionUseCase.Control.cpp` | 点胶业务编排阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `DispensingExecutionUseCase.cpp` | 点胶业务编排阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `DispensingExecutionUseCase.Setup.cpp` | 点胶业务编排阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `DispensingWorkflowUseCase.cpp` | 点胶业务编排阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `PlanningUseCase.cpp` | 点胶业务编排阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `UploadFileUseCase.cpp` | 点胶业务编排阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/application/usecases/dispensing/valve/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `ValveCommandUseCase.cpp` | 点胶执行前的阀控制编排阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 控制阀的开闭和联动，直接影响出胶开始、保持和结束。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `ValveQueryUseCase.cpp` | 点胶执行前的阀控制编排阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 控制阀的开闭和联动，直接影响出胶开始、保持和结束。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/application/usecases/dispensing/valve/coordination/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `ValveCoordinationUseCase.cpp` | 点胶执行前的阀联动协调阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 控制阀的开闭和联动，直接影响出胶开始、保持和结束。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/application/usecases/motion/coordination/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `MotionCoordinationUseCase.cpp` | 多轴协调编排阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/application/usecases/motion/homing/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `HomeAxesUseCase.cpp` | 正式点胶前的回零编排阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/application/usecases/motion/initialization/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `MotionInitializationUseCase.cpp` | 运动系统初始化阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/application/usecases/motion/interpolation/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `InterpolationPlanningUseCase.cpp` | 运动插补编排阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 把轨迹细化成控制周期可消费的插补数据。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/application/usecases/motion/manual/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `ManualMotionControlUseCase.cpp` | 人工干预/手动操作阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/application/usecases/motion/monitoring/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `MotionMonitoringUseCase.cpp` | 运动过程监控阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/application/usecases/motion/ptp/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `MoveToPositionUseCase.cpp` | 点到点移动编排阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/application/usecases/motion/runtime/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `MotionRuntimeAssemblyFactory.cpp` | 运动运行时控制阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 维持执行过程中的会话、状态和资源协同。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/application/usecases/motion/safety/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `MotionSafetyUseCase.cpp` | 运动安全编排阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 执行安全联锁或限位判断，防止设备在危险条件下继续动作。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/application/usecases/motion/trajectory/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `DeterministicPathExecutionUseCase.cpp` | 轨迹执行编排阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `ExecuteTrajectoryUseCase.cpp` | 轨迹执行编排阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 生成或调整轨迹，使运动控制器能够按预期走位。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/application/usecases/recipes/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `CompareRecipeVersionsUseCase.cpp` | 配方装载与切换阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 加载、校验或切换配方，使当前批次使用正确工艺参数。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `CreateDraftVersionUseCase.cpp` | 配方装载与切换阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `CreateRecipeUseCase.cpp` | 配方装载与切换阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 加载、校验或切换配方，使当前批次使用正确工艺参数。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `CreateVersionFromPublishedUseCase.cpp` | 配方装载与切换阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `ExportRecipeBundlePayloadUseCase.cpp` | 配方装载与切换阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 加载、校验或切换配方，使当前批次使用正确工艺参数。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `ImportRecipeBundlePayloadUseCase.cpp` | 配方装载与切换阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 加载、校验或切换配方，使当前批次使用正确工艺参数。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `RecipeCommandUseCase.cpp` | 配方装载与切换阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 加载、校验或切换配方，使当前批次使用正确工艺参数。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `RecipeQueryUseCase.cpp` | 配方装载与切换阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 加载、校验或切换配方，使当前批次使用正确工艺参数。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `UpdateDraftVersionUseCase.cpp` | 配方装载与切换阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `UpdateRecipeUseCase.cpp` | 配方装载与切换阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 加载、校验或切换配方，使当前批次使用正确工艺参数。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/application/usecases/redundancy/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `RedundancyGovernanceUseCases.cpp` | 冗余切换与恢复阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 处理冗余状态或切换逻辑，保证异常情况下流程可恢复。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/application/usecases/system/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `EmergencyStopUseCase.cpp` | 系统启动与生命周期管理阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `InitializeSystemUseCase.cpp` | 系统启动与生命周期管理阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/domain/domain/configuration/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `CMPPulseConfig.cpp` | 流程基础配置收敛阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 收敛设备、工艺或运行参数，为后续规划和执行提供边界条件。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `DispensingConfig.cpp` | 流程基础配置收敛阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 收敛设备、工艺或运行参数，为后续规划和执行提供边界条件。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `InterpolationConfig.cpp` | 流程基础配置收敛阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 收敛设备、工艺或运行参数，为后续规划和执行提供边界条件。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `MachineConfig.cpp` | 流程基础配置收敛阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 收敛设备、工艺或运行参数，为后续规划和执行提供边界条件。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `ValveTimingConfig.cpp` | 流程基础配置收敛阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 收敛设备、工艺或运行参数，为后续规划和执行提供边界条件。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/domain/domain/diagnostics/domain-services/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `ProcessResultSerialization.cpp` | 流程诊断判断阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `ProcessResultService.cpp` | 流程诊断判断阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/domain/domain/dispensing/domain-services/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `ArcTriggerPointCalculator.cpp` | 点胶领域规则计算阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 计算或下发点胶触发时机，决定胶何时开始和停止输出。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `CMPTriggerService.cpp` | 点胶领域规则计算阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 计算或下发点胶触发时机，决定胶何时开始和停止输出。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `DispenseCompensationService.cpp` | 点胶领域规则计算阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `DispensingController.cpp` | 点胶领域规则计算阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 把规划结果落实为受控动作或硬件指令。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `PathOptimizationStrategy.cpp` | 点胶领域规则计算阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `PurgeDispenserProcess.cpp` | 点胶领域规则计算阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `SupplyStabilizationPolicy.cpp` | 点胶领域规则计算阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `TriggerPlanner.cpp` | 点胶领域规则计算阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 计算或下发点胶触发时机，决定胶何时开始和停止输出。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `ValveCoordinationService.cpp` | 点胶领域规则计算阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 控制阀的开闭和联动，直接影响出胶开始、保持和结束。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/domain/domain/dispensing/planning/domain-services/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `ContourOptimizationService.cpp` | 点胶规划细化阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `DispensingPlannerService.cpp` | 点胶规划细化阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 把上游输入整理成下游可直接消费的规划结果。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `UnifiedTrajectoryPlannerService.cpp` | 点胶规划细化阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 把上游输入整理成下游可直接消费的规划结果。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/domain/domain/machine/aggregates/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `DispenserModel.cpp` | 设备状态建模阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 维护设备状态、当前任务、待执行任务和完成统计，判断设备是否允许进入正式点胶。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/domain/domain/machine/domain-services/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `CalibrationProcess.cpp` | 设备前置流程控制阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 推进校准请求的校验、执行、验证和结果收敛，确保后续坐标和落点可信。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/domain/domain/motion/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `BezierCalculator.cpp` | 运动领域实现阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `BSplineCalculator.cpp` | 运动领域实现阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `CircleCalculator.cpp` | 运动领域实现阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `CMPCompensation.cpp` | 运动领域实现阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `CMPCoordinatedInterpolator.cpp` | 运动领域实现阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `CMPValidator.cpp` | 运动领域实现阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/domain/domain/motion/domain-services/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `GeometryBlender.cpp` | 运动规则与轨迹求解阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `HomingProcess.cpp` | 运动规则与轨迹求解阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 执行回零与基准建立，确保正式点胶前的起始坐标正确。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `JogController.cpp` | 运动规则与轨迹求解阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 支持人工点动和微调，用于调试、对位或故障处理。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `MotionBufferController.cpp` | 运动规则与轨迹求解阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 把规划结果落实为受控动作或硬件指令。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `MotionControlServiceImpl.cpp` | 运动规则与轨迹求解阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `MotionStatusServiceImpl.cpp` | 运动规则与轨迹求解阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `SevenSegmentSCurveProfile.cpp` | 运动规则与轨迹求解阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `SpeedPlanner.cpp` | 运动规则与轨迹求解阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 把上游输入整理成下游可直接消费的规划结果。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `TimeTrajectoryPlanner.cpp` | 运动规则与轨迹求解阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 把上游输入整理成下游可直接消费的规划结果。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `TrajectoryPlanner.cpp` | 运动规则与轨迹求解阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 把上游输入整理成下游可直接消费的规划结果。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `TriggerCalculator.cpp` | 运动规则与轨迹求解阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 计算或下发点胶触发时机，决定胶何时开始和停止输出。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `VelocityProfileService.cpp` | 运动规则与轨迹求解阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/domain/domain/motion/domain-services/interpolation/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `ArcGeometryMath.cpp` | 插补计算阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `ArcInterpolator.cpp` | 插补计算阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `InterpolationCommandValidator.cpp` | 插补计算阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 把轨迹细化成控制周期可消费的插补数据。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `InterpolationProgramPlanner.cpp` | 插补计算阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 把上游输入整理成下游可直接消费的规划结果。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `LinearInterpolator.cpp` | 插补计算阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `SplineGeometryMath.cpp` | 插补计算阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `SplineInterpolator.cpp` | 插补计算阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `TrajectoryInterpolatorBase.cpp` | 插补计算阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 生成或调整轨迹，使运动控制器能够按预期走位。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `ValidatedInterpolationPort.cpp` | 插补计算阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 把轨迹细化成控制周期可消费的插补数据。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/domain/domain/recipes/domain-services/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `RecipeActivationService.cpp` | 配方领域规则阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 加载、校验或切换配方，使当前批次使用正确工艺参数。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `RecipeValidationService.cpp` | 配方领域规则阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 加载、校验或切换配方，使当前批次使用正确工艺参数。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/domain/domain/safety/domain-services/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `EmergencyStopService.cpp` | 联锁与软限位阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `InterlockPolicy.cpp` | 联锁与软限位阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 执行安全联锁或限位判断，防止设备在危险条件下继续动作。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `SoftLimitValidator.cpp` | 联锁与软限位阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/domain/domain/trajectory/domain-services/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `GeometryNormalizer.cpp` | 路径到轨迹转换阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `MotionPlanner.cpp` | 路径到轨迹转换阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 把上游输入整理成下游可直接消费的规划结果。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `PathRegularizer.cpp` | 路径到轨迹转换阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `ProcessAnnotator.cpp` | 路径到轨迹转换阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `SplineApproximation.cpp` | 路径到轨迹转换阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 承担该环节中的主要行为实现。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `TrajectoryShaper.cpp` | 路径到轨迹转换阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 生成或调整轨迹，使运动控制器能够按预期走位。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/domain/motion-core/src/safety/services/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `interlock_policy.cpp` | 运动核心安全服务阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 执行安全联锁或限位判断，防止设备在危险条件下继续动作。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/domain/process-core/src/recipes/services/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `recipe_activation_service.cpp` | 工艺核心配方服务阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 加载、校验或切换配方，使当前批次使用正确工艺参数。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `recipe_validation_service.cpp` | 工艺核心配方服务阶段 | 当上游任务已经进入系统，需要由工作流决定下一步流转和编排动作时触发。 | 加载、校验或切换配方，使当前批次使用正确工艺参数。 | 输出编排决定、领域结果、适配结果或用例执行结果，交给下一个流程环节。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/tests/process-runtime-core/unit/dispensing/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `CleanupFilesUseCaseTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 验证对应流程环节在正常和异常条件下是否符合预期。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `ContourPathOptimizerTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 验证对应流程环节在正常和异常条件下是否符合预期。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `DispensingExecutionUseCaseProgressTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 验证对应流程环节在正常和异常条件下是否符合预期。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `DispensingWorkflowUseCaseTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 验证对应流程环节在正常和异常条件下是否符合预期。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `PlanningRequestTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 验证对应流程环节在正常和异常条件下是否符合预期。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `UploadFileUseCaseTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 验证对应流程环节在正常和异常条件下是否符合预期。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/tests/process-runtime-core/unit/domain/diagnostics/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `ProcessResultServiceTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 验证对应流程环节在正常和异常条件下是否符合预期。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/tests/process-runtime-core/unit/domain/dispensing/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `ArcTriggerPointCalculatorTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 计算或下发点胶触发时机，决定胶何时开始和停止输出。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `ClosedLoopSkeletonTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 验证对应流程环节在正常和异常条件下是否符合预期。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `DispensingControllerTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 把规划结果落实为受控动作或硬件指令。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `PurgeDispenserProcessTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 验证对应流程环节在正常和异常条件下是否符合预期。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `TriggerPlannerTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 计算或下发点胶触发时机，决定胶何时开始和停止输出。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `ValveCoordinationServiceTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 控制阀的开闭和联动，直接影响出胶开始、保持和结束。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/tests/process-runtime-core/unit/domain/machine/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `CalibrationProcessTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 推进校准请求的校验、执行、验证和结果收敛，确保后续坐标和落点可信。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/tests/process-runtime-core/unit/domain/motion/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `JogControllerTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 支持人工点动和微调，用于调试、对位或故障处理。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `MotionBufferControllerTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 把规划结果落实为受控动作或硬件指令。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/tests/process-runtime-core/unit/domain/recipes/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `RecipeActivationServiceTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 加载、校验或切换配方，使当前批次使用正确工艺参数。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/tests/process-runtime-core/unit/domain/safety/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `EmergencyStopServiceTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 验证对应流程环节在正常和异常条件下是否符合预期。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `InterlockPolicyTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 执行安全联锁或限位判断，防止设备在危险条件下继续动作。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `SoftLimitValidatorTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 验证对应流程环节在正常和异常条件下是否符合预期。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/tests/process-runtime-core/unit/domain/trajectory/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `InterpolationProgramPlannerTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 把上游输入整理成下游可直接消费的规划结果。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `MotionPlannerConstraintTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 把上游输入整理成下游可直接消费的规划结果。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `MotionPlannerTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 把上游输入整理成下游可直接消费的规划结果。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `PathRegularizerTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 验证对应流程环节在正常和异常条件下是否符合预期。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/tests/process-runtime-core/unit/infrastructure/adapters/planning/dxf/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `DXFAdapterFactoryTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `PbPathSourceAdapterContractTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 把模块内部语义转换为外部设备、驱动或历史实现可执行的调用。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/tests/process-runtime-core/unit/infrastructure/adapters/redundancy/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `JsonRedundancyRepositoryAdapterTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 处理冗余状态或切换逻辑，保证异常情况下流程可恢复。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/tests/process-runtime-core/unit/motion/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `DeterministicPathExecutionUseCaseTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 验证对应流程环节在正常和异常条件下是否符合预期。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `HomeAxesUseCaseTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 验证对应流程环节在正常和异常条件下是否符合预期。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `HomingSupportTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 执行回零与基准建立，确保正式点胶前的起始坐标正确。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `MotionCoordinationUseCaseTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 验证对应流程环节在正常和异常条件下是否符合预期。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `MotionMonitoringUseCaseTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 验证对应流程环节在正常和异常条件下是否符合预期。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `MotionRuntimeAssemblyFactoryTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 维持执行过程中的会话、状态和资源协同。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `MotionSafetyUseCaseTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 执行安全联锁或限位判断，防止设备在危险条件下继续动作。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/tests/process-runtime-core/unit/recipes/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `RecipeCommandUseCaseTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 加载、校验或切换配方，使当前批次使用正确工艺参数。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/tests/process-runtime-core/unit/redundancy/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `CandidateScoringServiceTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 验证对应流程环节在正常和异常条件下是否符合预期。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
| `RedundancyGovernanceUseCasesTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 处理冗余状态或切换逻辑，保证异常情况下流程可恢复。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |

**workflow/tests/process-runtime-core/unit/system/**

| 文件 | 流程阶段 | 触发条件 | 当前职责 | 输出结果 | 失败影响 |
|---|---|---|---|---|---|
| `InitializeSystemUseCaseTest.cpp` | 流程回归验证阶段 | 当验证工作流各环节在测试场景下的表现时触发。 | 验证对应流程环节在正常和异常条件下是否符合预期。 | 输出测试断言或验证结果，证明工作流行为是否符合预期。 | 如果这里出错，整条点胶链会在顺序、状态流转或边界控制上出现问题。 |
