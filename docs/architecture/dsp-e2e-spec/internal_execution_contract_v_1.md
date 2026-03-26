# internal-execution-contract-v1

## 1. 目的

本文定义点胶机系统的**内部执行契约**，用于统一以下环节之间的数据边界、对象语义、引用关系与责任归属：

- DXF / CAD 导入与几何归一化
- 工艺路径规划
- 运动规划
- 执行段生成
- 控制器 / 驱动适配
- 运行时采集
- 执行追溯与诊断

本文**不**定义以下内容：

- 设备对外模式 / 状态接口
- MES / SCADA / OPC UA / PackML 对外语义
- 厂商私有板卡 API 细节
- 安全标准全文要求

本文目标不是替代具体算法实现，而是冻结**跨模块必须一致的契约层**。

---

## 2. 适用范围

本文适用于以下子系统：

- 上位机主流程
- 仿真引擎
- 板卡 / PLC / 控制器适配层
- 执行追溯与诊断模块
- HMI 只读展示层

本文覆盖以下核心对象：

1. `GeometryPrimitive`
2. `Contour`
3. `ProcessSegment`
4. `ProcessPath`
5. `MotionSegment`
6. `MotionPlan`
7. `IOTrigger`
8. `ValveTimingProfile`
9. `ExecutionSegment`
10. `ExecutionProgram`
11. `AxisRuntimeState`
12. `SegmentResult`

建议同步补充以下扩展对象：

- `CornerPolicy`
- `StartEndPolicy`
- `MaterialProfile`
- `VisionAlignmentTransform`
- `QualityEvidence`

---

## 3. 设计原则

### 3.1 单一真相来源

每类语义只能有一个主责任层：

- 几何真相：L2 几何层
- 工艺真相：L3 工艺路径层
- 运动真相：L4 运动规划层
- 可执行真相：L5 执行段层
- 底层状态真相：L6 控制器 / 驱动适配层
- 实际结果真相：L7 追溯层

### 3.2 显式引用优先于隐式顺序

所有跨层追溯**MUST** 使用显式引用字段完成。禁止仅依赖数组顺序推断上游来源。

### 3.3 下游不得静默篡改上游主语义

一旦某层对象被下游引用，下游**MUST NOT** 静默修改其主语义字段。若因约束无法满足，必须返回：

- 失败
- 降级
- 约束冲突报告

### 3.4 工艺与执行分离

`ProcessSegment` 表示“想怎么点”；`ExecutionSegment` 表示“控制器怎么执行”。两者**MUST** 分离，不得混层。

### 3.5 结果层独立存在

运行日志不能替代 `SegmentResult`。实际执行结果**MUST** 落成结构化对象。

---

## 4. 分层模型

### L0 安全包络层（横切）

职责：定义哪些动作根本不允许发生。

典型对象：

- `SafetySnapshot`
- `SafetyPermission`
- `SafetyInterlock`
- `SafetyFault`

规则：

- 任意执行动作前**MUST** 经过安全许可判断。
- 安全层不属于本文核心对象集，但本文所有执行对象都默认受其约束。

### L1 作业 / 配方层

职责：描述一次点胶任务的产品与工艺目标。

典型对象：

- `JobOrder`
- `Recipe`
- `OperationSpec`

边界：

- 本层不直接表达板卡程序段。
- 本层可引用工艺族、材料族、质量目标。

### L2 几何层

职责：表达图纸中的几何事实。

核心对象：

- `GeometryPrimitive`
- `Contour`

规则：

- 只描述几何，不描述点胶工艺。
- 不允许引入阀时序、胶宽、点距等工艺字段。

### L3 工艺路径层

职责：把几何转换为可点胶的工艺意图。

核心对象：

- `ProcessSegment`
- `ProcessPath`

规则：

- 本层是工艺真相唯一来源。
- 必须冻结点胶段、空移段、起止策略、角点策略等工艺语义。

### L4 运动规划层

职责：把工艺意图转换成带时间语义的运动计划。

核心对象：

- `MotionSegment`
- `MotionPlan`

规则：

- 本层负责“如何动得出来”。
- 本层不得擅自改写工艺层主语义。

### L5 执行段层

职责：把运动计划转换成控制器可执行对象。

核心对象：

- `IOTrigger`
- `ValveTimingProfile`
- `ExecutionSegment`
- `ExecutionProgram`

规则：

- 本层是控制器可执行真相唯一来源。
- 所有控制器专有翻译都必须能回链到本层对象。

### L6 控制器 / 驱动适配层

职责：统一屏蔽具体板卡、总线、驱动差异。

核心对象：

- `AxisRuntimeState`
- `ControllerAck`
- `ControllerFault`

规则：

- UI 与上层逻辑读取归一化状态，不直接读取厂商原始寄存器。

### L7 反馈 / 追溯层

职责：记录实际发生了什么。

核心对象：

- `SegmentResult`
- `ExecutionTrace`
- `AlarmContext`
- `DeviationReport`

规则：

- 本层是结果真相唯一来源。
- 不允许用计划值冒充实际值。

---

## 5. 全局规则

### 5.1 标识与版本

所有核心对象：

- **MUST** 带稳定 `id`
- **MUST** 带 `schema_version`

### 5.2 引用约定

所有跨层引用：

- **MUST** 使用 `*_ref` 或 `*_id`
- **MUST NOT** 用位置顺序隐式表达因果关系

### 5.3 错误语义

所有失败或异常：

- **MUST** 返回结构化错误码
- **MUST** 标注错误层级
- **MUST** 附带相关证据或引用

### 5.4 可追溯性

以下引用链**MUST** 成立：

- `MotionSegment.process_segment_ref`
- `ExecutionSegment.process_segment_ref`
- `SegmentResult.execution_segment_ref`

### 5.5 写权限

每个对象字段只能由指定角色写入；其他模块只能读取或派生，不得回写主语义字段。

---

## 6. 通用类型约定

| 类型 | 含义 |
|---|---|
| `id` | 全局唯一字符串 |
| `ref` | 其他对象 ID |
| `mm` | 毫米 |
| `deg` | 角度 |
| `ms` | 毫秒 |
| `timestamp` | ISO-8601 |
| `point2d` | `{x_mm, y_mm}` |
| `bbox2d` | `{min_x, min_y, max_x, max_y}` |
| `json` | 结构化扩展数据 |
| `enum` | 枚举字符串 |

---

## 7. 对象定义

## 7.1 GeometryPrimitive

职责：表达单个几何原语；不包含工艺语义。

### 字段

| 字段 | 类型 | 必填 | 写权限 | 说明 |
|---|---|---:|---|---|
| `id` | id | 是 | ingest | 原语唯一标识 |
| `schema_version` | string | 是 | ingest | 例如 `geometry-primitive/v1` |
| `source_file_id` | string | 是 | ingest | 来源文件 ID |
| `source_entity_id` | string | 是 | ingest | 来源实体 ID |
| `entity_type` | enum | 是 | ingest | `LINE/ARC/CIRCLE/SPLINE/POLYLINE/POINT` |
| `layer` | string | 否 | ingest | 图层 |
| `color` | string/int | 否 | ingest | 颜色或索引 |
| `source_units` | enum | 是 | ingest | 原始单位 |
| `normalized_units` | enum | 是 | geometry-normalizer | 归一化后单位，建议固定 `mm` |
| `geometry_payload` | json | 是 | geometry-normalizer | 原语参数 |
| `bbox` | bbox2d | 是 | geometry-normalizer | 包围盒 |
| `closed_flag` | bool | 否 | geometry-normalizer | 是否闭合 |
| `continuity_grade` | enum | 否 | geometry-normalizer | `UNKNOWN/G0/G1/G2` |
| `normalization_flags` | string[] | 否 | geometry-normalizer | 归一化动作记录 |
| `diagnostics` | json | 否 | geometry-normalizer | 异常信息 |

### 规则

- `GeometryPrimitive` **MUST NOT** 携带阀控、胶宽、点距、速度等级等工艺字段。
- 任何几何修复都**SHOULD** 记录到 `normalization_flags` 与 `diagnostics`。

---

## 7.2 Contour

职责：表达多个几何原语组成的轮廓或拓扑连续体。

### 字段

| 字段 | 类型 | 必填 | 写权限 | 说明 |
|---|---|---:|---|---|
| `id` | id | 是 | geometry-normalizer | 轮廓 ID |
| `schema_version` | string | 是 | geometry-normalizer | 版本 |
| `primitive_refs` | ref[] | 是 | geometry-normalizer | 原语引用 |
| `closed` | bool | 是 | geometry-normalizer | 是否闭环 |
| `winding` | enum | 否 | geometry-normalizer | `CW/CCW/UNKNOWN` |
| `bbox` | bbox2d | 是 | geometry-normalizer | 包围盒 |
| `topology_flags` | string[] | 否 | geometry-normalizer | 拓扑特征 |
| `self_intersection` | bool | 否 | geometry-normalizer | 是否自交 |
| `gap_report` | json | 否 | geometry-normalizer | 间隙报告 |
| `contour_role` | enum | 否 | process-planner | `OUTER/INNER/UNKNOWN` |

### 规则

- `Contour` 表达的是几何连续体，不表达点胶策略。
- `contour_role` 可由工艺层补充，但不得反向改写几何事实。

---

## 7.3 ProcessSegment

职责：工艺层最小执行意图单元。

### 字段

| 字段 | 类型 | 必填 | 写权限 | 说明 |
|---|---|---:|---|---|
| `id` | id | 是 | process-planner | 工艺段 ID |
| `schema_version` | string | 是 | process-planner | 版本 |
| `contour_ref` | ref | 否 | process-planner | 来源轮廓 |
| `geometry_refs` | ref[] | 否 | process-planner | 来源原语 |
| `segment_kind` | enum | 是 | process-planner | `DISPENSE/TRAVEL/APPROACH/LEAVE/PURGE/WAIT` |
| `path_curve` | json | 是 | process-planner | 工艺路径表达 |
| `dispense_enabled` | bool | 是 | process-planner | 是否允许出胶 |
| `target_pitch_mm` | mm | 否 | process-planner | 目标点距 / 采样距 |
| `target_bead_width_mm` | mm | 否 | process-planner | 目标胶宽 |
| `speed_class` | enum | 否 | process-planner | `LOW/NORMAL/HIGH/CUSTOM` |
| `corner_policy_ref` | ref | 否 | process-planner | 角点策略 |
| `start_end_policy_ref` | ref | 否 | process-planner | 起止策略 |
| `material_profile_ref` | ref | 否 | process-planner | 材料画像 |
| `quality_constraints` | json | 否 | process-planner | 质量约束 |
| `planning_notes` | json | 否 | process-planner | 工艺备注 |

### 规则

- `ProcessSegment` 是工艺真相对象。
- `dispense_enabled=false` 的段，下游**MUST NOT** 自动变为出胶段。
- 若下游无法满足本段要求，必须报告失败或降级，不得静默吞掉。

---

## 7.4 ProcessPath

职责：组织完整工艺路径与工艺段顺序。

### 字段

| 字段 | 类型 | 必填 | 写权限 | 说明 |
|---|---|---:|---|---|
| `id` | id | 是 | process-planner | 路径 ID |
| `schema_version` | string | 是 | process-planner | 版本 |
| `job_id` | string | 是 | process-planner | 所属作业 |
| `recipe_id` | string | 否 | process-planner | 所属配方 |
| `segment_refs` | ref[] | 是 | process-planner | 工艺段序列 |
| `path_role` | enum | 否 | process-planner | `MAIN/ASSIST/CLEAN/CALIBRATION` |
| `coordinate_frame` | string | 是 | process-planner | 坐标系 |
| `vision_transform_ref` | ref | 否 | process-planner | 视觉变换 |
| `estimated_total_length_mm` | mm | 否 | process-planner | 估计总长度 |
| `estimated_dispense_length_mm` | mm | 否 | process-planner | 估计出胶长度 |
| `process_constraints` | json | 否 | process-planner | 路径级约束 |

### 规则

- `ProcessPath` 是工艺层聚合根。
- 任何视觉修正只允许通过显式变换引用进入，不得静默改写原始几何来源。

---

## 7.5 MotionSegment

职责：带动力学与时间语义的运动段。

### 字段

| 字段 | 类型 | 必填 | 写权限 | 说明 |
|---|---|---:|---|---|
| `id` | id | 是 | motion-planner | 运动段 ID |
| `schema_version` | string | 是 | motion-planner | 版本 |
| `process_segment_ref` | ref | 是 | motion-planner | 来源工艺段 |
| `axis_group_id` | string | 是 | motion-planner | 坐标组 |
| `motion_type` | enum | 是 | motion-planner | `LINEAR/CIRCULAR/SPLINE/DWELL/TRIGGERED_MOVE` |
| `geometric_path_ref` | ref/json | 是 | motion-planner | 几何路径引用或副本 |
| `vmax_mm_s` | number | 否 | motion-planner | 最大速度 |
| `amax_mm_s2` | number | 否 | motion-planner | 最大加速度 |
| `jmax_mm_s3` | number | 否 | motion-planner | 最大跃度 |
| `blend_in` | json | 否 | motion-planner | 入段混接 |
| `blend_out` | json | 否 | motion-planner | 出段混接 |
| `planned_length_mm` | mm | 是 | motion-planner | 规划长度 |
| `planned_duration_ms` | ms | 否 | motion-planner | 规划时长 |
| `constraint_status` | enum | 是 | motion-planner | `OK/DEGRADED/FAILED` |
| `constraint_report` | json | 否 | motion-planner | 冲突说明 |
| `trigger_window_defs` | json | 否 | motion-planner | 触发窗口 |

### 规则

- `MotionSegment.process_segment_ref` **MUST EXIST**。
- 若 `constraint_status=DEGRADED`，必须提供 `constraint_report`。
- 运动层不得静默改写工艺层的目标点距、胶宽、段类型。

---

## 7.6 MotionPlan

职责：组织完整运动规划结果。

### 字段

| 字段 | 类型 | 必填 | 写权限 | 说明 |
|---|---|---:|---|---|
| `id` | id | 是 | motion-planner | 运动计划 ID |
| `schema_version` | string | 是 | motion-planner | 版本 |
| `process_path_ref` | ref | 是 | motion-planner | 来源工艺路径 |
| `segment_refs` | ref[] | 是 | motion-planner | 运动段序列 |
| `axis_group_id` | string | 是 | motion-planner | 主坐标组 |
| `homing_required` | bool | 否 | motion-planner | 是否要求回零 |
| `kinematic_profile` | json | 否 | motion-planner | 运动学配置 |
| `estimated_cycle_time_ms` | ms | 否 | motion-planner | 估计节拍 |
| `planning_status` | enum | 是 | motion-planner | `OK/DEGRADED/FAILED` |
| `planning_report` | json | 否 | motion-planner | 报告 |

### 规则

- `MotionPlan` 只表达规划结果，不表达板卡私有下发格式。

---

## 7.7 IOTrigger

职责：表达 IO 动作与位置 / 时间 / 段边界的绑定关系。

### 字段

| 字段 | 类型 | 必填 | 写权限 | 说明 |
|---|---|---:|---|---|
| `id` | id | 是 | execution-planner | 触发器 ID |
| `schema_version` | string | 是 | execution-planner | 版本 |
| `source_type` | enum | 是 | execution-planner | `POSITION/TIME/SEGMENT_START/SEGMENT_END` |
| `channel` | enum | 是 | execution-planner | `CMP/PURGE/CAMERA/MARKER/DO_xxx` |
| `action` | enum | 是 | execution-planner | `ON/OFF/PULSE` |
| `offset_type` | enum | 否 | execution-planner | `DISTANCE/TIME/NONE` |
| `offset_value` | number | 否 | execution-planner | 偏移量 |
| `pulse_width_ms` | ms | 否 | execution-planner | 脉冲宽度 |
| `safety_condition_ref` | ref | 否 | execution-planner | 安全前提 |
| `notes` | json | 否 | execution-planner | 备注 |

### 规则

- 阀类触发**SHOULD** 显式声明偏移类型。
- 禁止仅靠段顺序推断全部 IO 语义。

---

## 7.8 ValveTimingProfile

职责：描述阀控与运动之间的补偿策略。

### 字段

| 字段 | 类型 | 必填 | 写权限 | 说明 |
|---|---|---:|---|---|
| `id` | id | 是 | execution-planner | 画像 ID |
| `schema_version` | string | 是 | execution-planner | 版本 |
| `material_profile_ref` | ref | 否 | execution-planner | 材料画像 |
| `pre_open_ms` | ms | 否 | execution-planner | 提前开阀 |
| `post_close_ms` | ms | 否 | execution-planner | 延后关阀 |
| `suckback_ms` | ms | 否 | execution-planner | 回吸时长 |
| `corner_compensation_mode` | enum | 否 | execution-planner | `NONE/SLOWDOWN/OVERLAP/DWELL/CUSTOM` |
| `start_boost_ms` | ms | 否 | execution-planner | 起胶增强 |
| `end_trim_ms` | ms | 否 | execution-planner | 收尾修剪 |
| `profile_status` | enum | 是 | execution-planner | `DEFAULT/TUNED/EXPERIMENTAL` |
| `evidence_refs` | ref[] | 否 | execution-planner | 调参依据 |

### 规则

- 本对象只表达策略，不直接操作驱动层。
- 本对象必须通过 `IOTrigger` 或 `ExecutionSegment` 落地。

---

## 7.9 ExecutionSegment

职责：控制器真正可执行的最小段对象。

### 字段

| 字段 | 类型 | 必填 | 写权限 | 说明 |
|---|---|---:|---|---|
| `id` | id | 是 | execution-planner | 执行段 ID |
| `schema_version` | string | 是 | execution-planner | 版本 |
| `process_segment_ref` | ref | 是 | execution-planner | 来源工艺段 |
| `motion_segment_ref` | ref | 是 | execution-planner | 来源运动段 |
| `controller_command_type` | enum | 是 | execution-planner | 控制器命令类型 |
| `sequence_no` | int | 是 | execution-planner | 顺序号 |
| `axis_targets` | json | 否 | execution-planner | 轴目标 |
| `feedrate_mm_s` | number | 否 | execution-planner | 速度 |
| `accel_profile` | json | 否 | execution-planner | 加减速参数 |
| `io_trigger_refs` | ref[] | 否 | execution-planner | 触发器列表 |
| `valve_timing_profile_ref` | ref | 否 | execution-planner | 阀时序画像 |
| `expected_controller_ack` | enum | 否 | execution-planner | 预期 ACK |
| `checksum_scope` | string | 否 | execution-planner | 校验范围 |
| `adapter_hints` | json | 否 | execution-planner | 适配器提示 |

### 规则

- `ExecutionSegment.process_segment_ref` **MUST EXIST**。
- 不允许仅依赖 `sequence_no` 维持工艺追溯。
- 所有控制器专有翻译都必须可回溯到本对象。

---

## 7.10 ExecutionProgram

职责：组织可一次性或分批下发给控制器的执行程序。

### 字段

| 字段 | 类型 | 必填 | 写权限 | 说明 |
|---|---|---:|---|---|
| `id` | id | 是 | execution-planner | 程序 ID |
| `schema_version` | string | 是 | execution-planner | 版本 |
| `motion_plan_ref` | ref | 是 | execution-planner | 来源运动计划 |
| `segment_refs` | ref[] | 是 | execution-planner | 执行段序列 |
| `controller_type` | string | 是 | execution-planner | 目标控制器 |
| `download_mode` | enum | 否 | execution-planner | `FULL/STREAMING/BATCH` |
| `program_checksum` | string | 否 | execution-planner | 校验值 |
| `estimated_cycle_time_ms` | ms | 否 | execution-planner | 估计节拍 |
| `execution_preconditions` | json | 否 | execution-planner | 执行前提 |
| `program_status` | enum | 是 | execution-planner | `READY/INVALID/BLOCKED` |
| `validation_report` | json | 否 | execution-planner | 校验报告 |

### 规则

- `ExecutionProgram` 是 L5 聚合根。
- 板卡下发格式可以不同，但必须从本对象可导出。

---

## 7.11 AxisRuntimeState

职责：屏蔽驱动 / 板卡差异后的统一轴运行态。

### 字段

| 字段 | 类型 | 必填 | 写权限 | 说明 |
|---|---|---:|---|---|
| `id` | id | 是 | controller-adapter | 状态对象 ID |
| `schema_version` | string | 是 | controller-adapter | 版本 |
| `axis_id` | string | 是 | controller-adapter | 轴号 |
| `power_state` | enum | 是 | controller-adapter | `OFF/ON/FAULT` |
| `home_state` | enum | 是 | controller-adapter | `UNKNOWN/NOT_HOMED/HOMING/HOMED` |
| `op_mode` | enum | 否 | controller-adapter | 运行模式 |
| `position_actual_mm` | number | 否 | runtime | 实际位置 |
| `velocity_actual_mm_s` | number | 否 | runtime | 实际速度 |
| `following_error_mm` | number | 否 | runtime | 跟随误差 |
| `positive_limit` | bool | 否 | runtime | 正限位 |
| `negative_limit` | bool | 否 | runtime | 负限位 |
| `alarm_code` | string | 否 | controller-adapter | 归一化报警码 |
| `vendor_alarm_code` | string | 否 | controller-adapter | 厂商报警码 |
| `updated_at` | timestamp | 是 | runtime | 更新时间 |

### 规则

- 上层只能读取归一化态，不直接读取厂商底层寄存器。
- 任何厂商专有状态都**SHOULD** 映射到统一枚举或统一字段。

---

## 7.12 SegmentResult

职责：记录执行后的事实结果。

### 字段

| 字段 | 类型 | 必填 | 写权限 | 说明 |
|---|---|---:|---|---|
| `id` | id | 是 | trace-service | 结果 ID |
| `schema_version` | string | 是 | trace-service | 版本 |
| `execution_segment_ref` | ref | 是 | trace-service | 来源执行段 |
| `started_at` | timestamp | 否 | runtime | 开始时间 |
| `ended_at` | timestamp | 否 | runtime | 结束时间 |
| `actual_duration_ms` | ms | 否 | runtime | 实际时长 |
| `actual_path_length_mm` | mm | 否 | runtime | 实际长度 |
| `controller_ack` | enum/string | 否 | controller-adapter | 实际 ACK |
| `io_fired` | json | 否 | runtime | 实际触发列表 |
| `alarm_refs` | ref[] | 否 | trace-service | 报警引用 |
| `completed` | bool | 是 | trace-service | 是否完成 |
| `completion_code` | string | 是 | trace-service | 完成码 |
| `deviation_report` | json | 否 | trace-service | 偏差报告 |
| `quality_evidence_refs` | ref[] | 否 | trace-service | 质量证据 |

### 规则

- `SegmentResult.execution_segment_ref` **MUST EXIST**。
- 本对象是“实际发生了什么”的唯一权威层。
- 不允许用计划值冒充本对象字段。

---

## 8. 扩展对象定义

## 8.1 CornerPolicy

| 字段 | 类型 | 必填 | 说明 |
|---|---|---:|---|
| `id` | id | 是 | 策略 ID |
| `corner_type` | enum | 是 | 拐角类型 |
| `min_angle_deg` | deg | 否 | 最小角度阈值 |
| `slow_down_factor` | number | 否 | 降速系数 |
| `dwell_ms` | ms | 否 | 停顿时间 |
| `overlap_mm` | mm | 否 | 重叠长度 |
| `valve_compensation_mode` | enum | 否 | 阀补偿模式 |

## 8.2 StartEndPolicy

| 字段 | 类型 | 必填 | 说明 |
|---|---|---:|---|
| `id` | id | 是 | 策略 ID |
| `lead_in_mm` | mm | 否 | 入段预引 |
| `lead_out_mm` | mm | 否 | 出段收尾 |
| `pre_open_ms` | ms | 否 | 提前开阀 |
| `post_close_ms` | ms | 否 | 延后关阀 |
| `suckback_ms` | ms | 否 | 回吸 |
| `tail_trim_mode` | enum | 否 | 尾部处理 |

## 8.3 MaterialProfile

| 字段 | 类型 | 必填 | 说明 |
|---|---|---:|---|
| `id` | id | 是 | 材料画像 ID |
| `glue_type` | string | 是 | 胶型 |
| `viscosity_grade` | string | 否 | 黏度等级 |
| `pressure_nominal` | number | 否 | 标称压力 |
| `pressure_range` | json | 否 | 压力范围 |
| `recommended_speed_window` | json | 否 | 推荐速度窗口 |
| `valve_delay_model_ref` | ref | 否 | 阀延迟模型 |

## 8.4 VisionAlignmentTransform

| 字段 | 类型 | 必填 | 说明 |
|---|---|---:|---|
| `id` | id | 是 | 变换 ID |
| `source_fiducials` | json | 是 | 原始基准点 |
| `target_fiducials` | json | 是 | 目标基准点 |
| `transform_type` | enum | 是 | `RIGID/AFFINE/SIMILARITY/...` |
| `matrix` | json | 是 | 变换矩阵 |
| `residual_error_mm` | mm | 否 | 残差 |
| `accepted` | bool | 是 | 是否通过 |

## 8.5 QualityEvidence

| 字段 | 类型 | 必填 | 说明 |
|---|---|---:|---|
| `id` | id | 是 | 证据 ID |
| `camera_snapshot_refs` | ref[] | 否 | 图像引用 |
| `measured_bead_width_mm` | number[] | 否 | 测量胶宽 |
| `continuity_score` | number | 否 | 连续性评分 |
| `missing_glue_regions` | json | 否 | 缺胶区域 |
| `excess_glue_regions` | json | 否 | 堆胶区域 |

---

## 9. 强制引用链

以下主链**MUST** 成立：

```text
GeometryPrimitive
 -> Contour
 -> ProcessSegment
 -> ProcessPath
 -> MotionSegment
 -> MotionPlan
 -> IOTrigger / ValveTimingProfile
 -> ExecutionSegment
 -> ExecutionProgram
 -> AxisRuntimeState
 -> SegmentResult
```

以下强引用不可省略：

```text
MotionSegment.process_segment_ref
ExecutionSegment.process_segment_ref
ExecutionSegment.motion_segment_ref
SegmentResult.execution_segment_ref
```

---

## 10. 错误码框架

建议统一错误对象：

| 字段 | 类型 | 必填 | 说明 |
|---|---|---:|---|
| `error_code` | string | 是 | 结构化错误码 |
| `error_layer` | enum | 是 | `GEOMETRY/PROCESS/MOTION/EXECUTION/ADAPTER/RUNTIME/TRACE` |
| `severity` | enum | 是 | `INFO/WARN/ERROR/FATAL` |
| `message` | string | 是 | 面向日志 |
| `evidence` | json | 否 | 证据 |
| `related_refs` | ref[] | 否 | 关联对象 |

命名建议：

- `GEOM_*`
- `PROC_*`
- `MOTION_*`
- `EXEC_*`
- `ADAPTER_*`
- `RUNTIME_*`
- `TRACE_*`

---

## 11. 必须优先冻结的字段

以下字段建议作为第一批冻结对象：

1. `ProcessSegment.segment_kind`
2. `ProcessSegment.dispense_enabled`
3. `ProcessSegment.target_pitch_mm`
4. `ProcessSegment.target_bead_width_mm`
5. `ProcessSegment.corner_policy_ref`
6. `ProcessSegment.start_end_policy_ref`
7. `MotionSegment.process_segment_ref`
8. `MotionSegment.motion_type`
9. `MotionSegment.constraint_status`
10. `IOTrigger.source_type`
11. `IOTrigger.channel`
12. `IOTrigger.action`
13. `ValveTimingProfile.pre_open_ms`
14. `ValveTimingProfile.post_close_ms`
15. `ExecutionSegment.process_segment_ref`
16. `ExecutionSegment.motion_segment_ref`
17. `ExecutionSegment.sequence_no`
18. `SegmentResult.completion_code`

---

## 12. 明确禁止事项

以下行为在本契约下**MUST NOT** 发生：

1. 在几何层引入工艺字段
2. 在运动层静默改写工艺段目标
3. 在执行层丢失 `process_segment_ref`
4. 用执行段顺序隐式代表全部工艺关系
5. UI 直接读取驱动私有寄存器作为业务真相
6. 用日志文本替代 `SegmentResult`
7. 在运行时发现不可执行却不形成结构化失败对象

---

## 13. 最小落地步骤

### P0：先冻结边界

必须先写清：

- L2 只负责几何真相
- L3 是工艺真相唯一来源
- L5 是控制器可执行真相唯一来源
- 所有执行追溯必须能回链到 `ProcessSegment`

### P1：先冻结对象，再冻结算法

先冻结：

- 字段名
- 字段类型
- 必填项
- 引用关系
- 枚举值
- 错误码

后讨论：

- 采样算法
- 角点补偿算法
- 阀时序调优算法
- 板卡优化策略

### P2：先打通 3 条纵向用例

至少覆盖：

1. 直线连续点胶
2. 小半径拐角点胶
3. 断胶 + 空移 + 再起胶

### P3：最后做板卡适配

先冻结 `ExecutionSegment`，再让不同适配器去落地。

---

## 14. 建议的代码映射产物

建议同步产出：

### 文档 A

`docs/architecture/internal-execution-contract-v1.md`

用途：

- 冻结对象与边界
- 作为跨模块共同语言

### 文档 B

`docs/architecture/internal-execution-code-map-v1.md`

用途：

- 映射当前代码中的结构体 / DTO / 实体
- 标记字段缺失项
- 标记命名误导项
- 标记临时兼容策略

---

## 15. 结论

本文定义的不是完整实现，而是**内部执行契约骨架**。

在该骨架下：

- 工艺语义有唯一来源
- 运动语义有唯一来源
- 执行语义有唯一来源
- 结果语义有唯一来源
- 任一执行结果都可以向上追溯到工艺段

后续实现、重构、仿真与板卡替换，都不得突破本文规定的主边界。

