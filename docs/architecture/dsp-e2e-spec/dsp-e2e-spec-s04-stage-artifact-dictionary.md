# 《点胶机端到端流程规格说明 s04：阶段产物数据字典（对象清单，修订版）》

本版在原《s04 阶段产物数据字典》基础上合并两份正式评估的修订意见，目标不是增加更多对象，而是把以下 6 个高风险缺口彻底补齐：

1. 统一对象引用与版本链表达，避免 `artifact_ref` 与 `artifact_id/artifact_version` 两套口径并存。
2. 明确 `AlignmentCompensation` 的 **NoOp / NotRequired** 语义，消除“无需对位却要求必填补偿引用”的冲突。
3. 将 `FirstArticleResult` 的失败/豁免场景改为 **条件必填**，保证回退分流和放行门禁可执行。
4. 明确 `MachineReadySnapshot`、`ExecutionPackage`、`ExecutionRecord` 的关键追溯字段。
5. 把 `superseded / archived` 从“对象状态字面值”升级为“可执行的 owner 责任规则”。
6. 为 s05 / s07 / s09 提供统一的数据字典锚点。

---

# 1. 总体对象设计规则

## 1.1 对象公共字段（所有阶段产物统一遵守）

建议所有阶段产物至少具备以下公共字段：

| 字段 | 说明 |
|---|---|
| `id` | 对象主键 |
| `version` | 对象版本号，禁止回写旧版本 |
| `artifact_type` | 对象类型，例如 `ProcessPlan`、`ExecutionPackage` |
| `job_id` | 归属任务 |
| `stage_id` | 首次生成该对象的阶段 |
| `producer` | 生成该对象的 owner 模块 |
| `created_at` | 生成时间 |
| `input_refs` | 上游对象引用列表 |
| `status` | `valid / invalid / superseded / archived` |
| `diagnostics` | 诊断信息区，包括 `warnings / reject_reasons / stats / debug_annotations` |

说明：
- `status` 只表达对象生命周期，不替代工作流状态机。
- `diagnostics` 与业务事实字段必须分区，禁止混写。

## 1.2 对象引用规范

跨对象引用统一使用：

```text
artifact_ref = {
  artifact_type,
  artifact_id,
  artifact_version
}
```

约束：
- 同一文档内允许为了阅读简洁写 `process_plan_ref`、`execution_package_ref` 等专用字段名。
- 专用字段的值结构仍然必须符合 `artifact_ref`。
- 后续 s05 的命令/事件契约与 s07 的总线公共字段一律引用此结构，不再分别定义一套 `artifact_id` / `artifact_version` 的松散字段。

## 1.3 对象默认不可原地修改

对象一律采用“新建新版本”的方式变更，而不是覆盖历史对象。

必须禁止：
- 原地改写 `ProcessPlan`、`MotionPlan`、`ExecutionPackage`
- 运行时临时修改冻结包后又不留下版本痕迹
- 用追溯结果回写历史事实对象

## 1.4 对象只承载本层语义

必须避免以下污染：
- 几何对象里塞工艺参数
- 工艺对象里塞设备私有控制字
- 轨迹对象里塞阀时序事件
- 执行包里塞运行时临时改写字段
- 追溯对象里反向修正上游对象事实

## 1.5 条件必填规则

除“必填 / 可选”之外，统一引入 **条件必填**：

- 当某对象处于特定结果状态时，相关字段必须存在。
- 条件必填字段缺失时，该对象一律视为 **结构不合法**。
- 条件必填优先级高于“可选字段”。

## 1.6 NoOp / NotRequired 对象规则

为避免“某阶段逻辑被跳过，但下游又要求强引用”的冲突，允许定义 **NoOp / NotRequired** 对象。

本规格中首先应用于：
- `AlignmentCompensation`

规则如下：
- 即便当前任务无需视觉/测高补偿，M5 仍应生成一个 `AlignmentCompensation` 对象。
- 该对象的 `compensation_mode = none`，`compensation_result = not_required`。
- 下游 `ProcessPath` 继续强制引用 `alignment_compensation_ref`，从而保持契约闭合和版本链完整。

## 1.7 superseded / archived 的执行规则

- `superseded` 不只是状态值，还表示“该对象被某个新版本或回退动作正式替代”。
- `archived` 表示该对象已进入归档平面，仍可追溯但不再作为活动版本参与执行。
- 任何对象进入 `superseded` 或 `archived`，都必须由该对象的 owner 模块执行，并产生可追溯事件。

## 1.8 回退时的通用对象失效规则

当回退目标落在某一阶段时：
- 回退目标之前的对象保留 `valid`
- 回退目标之后的对象全部由各自 owner 模块标记为 `superseded`
- 重新生成的新对象必须带新版本号，且 `input_refs` 必须能回链到触发回退后的上游版本

---

# 2. 对象清单与数据字典

以下仅保留系统级必须对齐的字段与约束，避免把模块内部临时结构混入对象字典。

## 2.1 `JobDefinition`

### 对象职责
表示一次制造任务的顶层业务上下文，回答“要做什么、在哪台设备上做、由谁发起”。

### 必填字段
- `id`
- `version`
- `artifact_type = JobDefinition`
- `job_id`
- `product_code`
- `part_type`
- `fixture_rev`
- `material_code`
- `process_template_ref`
- `machine_id`
- `operator_id`
- `source_file_name`
- `created_at`

### 可选字段
- `work_order_id`
- `batch_id`
- `customer_code`
- `priority`
- `shift_code`
- `nozzle_id`
- `process_template_id`
- `notes`
- `tags`

### 不允许承载的内容
- DXF 解析结果
- 几何段集合
- 工艺路径
- 运动时序
- 运行时实时状态

### 与上下游关系
- 上游：外部工单系统 / HMI 输入
- 下游：`SourceDrawing`

---

## 2.2 `SourceDrawing`

### 对象职责
表示原始图纸事实源，只负责“输入了什么文件”，不负责解释几何含义。

### 必填字段
- `id`
- `version`
- `artifact_type = SourceDrawing`
- `job_id`
- `file_name`
- `file_hash`
- `file_size`
- `file_format`
- `storage_uri` 或 `blob_ref`
- `ingest_result`
- `created_at`

### 可选字段
- `dxf_version`
- `encoding`
- `layer_summary`
- `entity_count_raw`
- `block_count_raw`
- `warnings`

### 不允许承载的内容
- 规范几何对象
- 拓扑修复结果
- 工艺解释
- 设备补偿结果

### 与上下游关系
- 上游：`JobDefinition`
- 下游：`CanonicalGeometry`

---

## 2.3 `CanonicalGeometry`

### 对象职责
表示 DXF 解析并标准化后的内部几何模型。

### 必填字段
- `id`
- `version`
- `artifact_type = CanonicalGeometry`
- `job_id`
- `source_drawing_ref`
- `unit_mode`
- `geometry_entities`
- `entity_count`
- `parse_result`
- `created_at`

### `geometry_entities` 建议子字段
- `entity_id`
- `entity_type`
- `source_layer`
- `normalized_points`
- `bbox`
- `closed_flag`
- `raw_entity_ref`

### 可选字段
- `unsupported_entities`
- `degenerate_entities`
- `stats`
- `warnings`

### 不允许承载的内容
- 工艺模板
- 速度与阀延时
- 设备控制指令
- 运行时补偿结果

### 与上下游关系
- 上游：`SourceDrawing`
- 下游：`TopologyModel`

---

## 2.4 `TopologyModel`

### 对象职责
表示几何修复、连通性重建与拓扑结构。

### 必填字段
- `id`
- `version`
- `artifact_type = TopologyModel`
- `job_id`
- `canonical_geometry_ref`
- `chains`
- `loops`
- `connectivity_graph`
- `repair_result`
- `created_at`

### 可选字段
- `repair_actions`
- `gap_summary`
- `self_intersection_summary`
- `warnings`

### 不允许承载的内容
- 工艺策略
- 设备动态参数
- 阀时序
- 实时设备门禁状态

### 与上下游关系
- 上游：`CanonicalGeometry`
- 下游：`FeatureGraph`

---

## 2.5 `FeatureGraph`

### 对象职责
表示制造特征抽取结果，回答“哪些区域需要点胶、按什么特征分类”。

### 必填字段
- `id`
- `version`
- `artifact_type = FeatureGraph`
- `job_id`
- `topology_model_ref`
- `features`
- `feature_count`
- `classification_result`
- `created_at`

### `features` 建议子字段
- `feature_id`
- `feature_type`
- `topology_refs`
- `priority`
- `manufacturing_zone`
- `coverage_ratio`

### 可选字段
- `unclassified_regions`
- `conflicts`
- `warnings`

### 不允许承载的内容
- 速度、加速度、jerk
- 阀开关策略
- 设备报警信息

### 与上下游关系
- 上游：`TopologyModel`
- 下游：`ProcessPlan`

---

## 2.6 `ProcessPlan`

### 对象职责
表示工艺规则映射结果，回答“每个特征应该怎么做”。

### 必填字段
- `id`
- `version`
- `artifact_type = ProcessPlan`
- `job_id`
- `feature_graph_ref`
- `material_profile_ref`
- `process_items`
- `process_rule_result`
- `created_at`

### `process_items` 建议子字段
- `process_item_id`
- `feature_ref`
- `dispense_mode`
- `target_speed_profile`
- `start_stop_strategy`
- `coverage_strategy`
- `z_policy`

### 可选字段
- `template_refs`
- `quality_limits`
- `warnings`

### 不允许承载的内容
- 运动学插补实现细节
- 运行时设备门禁状态
- 执行中的 fault 历史

### 与上下游关系
- 上游：`FeatureGraph`
- 下游：`CoordinateTransformSet`、`ProcessPath`、`DispenseTimingPlan`

---

## 2.7 `CoordinateTransformSet`

### 对象职责
表示设计坐标到工艺/治具/设备坐标的静态变换链。

### 必填字段
- `id`
- `version`
- `artifact_type = CoordinateTransformSet`
- `job_id`
- `process_plan_ref`
- `design_to_process_transform`
- `process_to_fixture_transform`
- `fixture_to_machine_transform`
- `transform_result`
- `created_at`

### 可选字段
- `mirror_mode`
- `rotation_deg`
- `scale_x`
- `scale_y`
- `origin_offset`
- `fixture_origin`
- `machine_origin`
- `workspace_check_result`
- `warnings`

### 不允许承载的内容
- 动态视觉实测补偿值
- 针嘴校准运行时偏移
- 工艺路径排序结果
- 轨迹动态参数

### 与上下游关系
- 上游：`ProcessPlan`
- 下游：`AlignmentCompensation`、`ProcessPath`

---

## 2.8 `AlignmentCompensation`

### 对象职责
表示执行前获得的动态补偿结果，或者显式声明“不需要补偿”。

### 必填字段
- `id`
- `version`
- `artifact_type = AlignmentCompensation`
- `job_id`
- `coordinate_transform_set_ref`
- `compensation_mode`（`vision / height / nozzle / mixed / none`）
- `correction_vector`（`dx / dy / dz / theta`）
- `compensation_result`（`applied / not_required / rejected`）
- `created_at`

### 条件必填
- 当 `compensation_mode != none` 时：`fiducial_points` 或 `measurement_refs` 至少一类必须存在。
- 当 `compensation_result = not_required` 时：
  - `compensation_mode` 必须为 `none`
  - `not_required_reason` 必填
  - `correction_vector` 必须全部为零
- 当 `compensation_result = rejected` 时：`failure_code`、`recommended_action` 必填。

### 可选字段
- `fiducial_points`
- `camera_offset`
- `nozzle_offset`
- `height_map_ref`
- `compensation_limits`
- `not_required_reason`
- `failure_code`
- `recommended_action`
- `warnings`

### 不允许承载的内容
- 修改 `CoordinateTransformSet`
- 修改 `ProcessPlan`
- 修改 `SourceDrawing`
- 轨迹动态参数

### 与上下游关系
- 上游：`CoordinateTransformSet`
- 下游：`ProcessPath`
- 关键规则：即使无需补偿，也必须生成一版 `AlignmentCompensation`，供 `ProcessPath` 强引用。

---

## 2.9 `ProcessPath`

### 对象职责
表示按工艺语义组织后的执行路径，回答“先走哪条、后走哪条、哪些段出胶、哪些段空跑”。

### 必填字段
- `id`
- `version`
- `artifact_type = ProcessPath`
- `job_id`
- `process_plan_ref`
- `coordinate_transform_set_ref`
- `alignment_compensation_ref`
- `path_segments`
- `path_order`
- `process_path_result`
- `created_at`

### `path_segments` 建议子字段
- `segment_id`
- `feature_ref`
- `segment_role`（`dispense / travel / approach / depart`）
- `geometry_ref`
- `start_point_machine`
- `end_point_machine`
- `z_policy`
- `sequence_index`

### 可选字段
- `group_id`
- `approach_strategy`
- `depart_strategy`
- `travel_clearance_z`
- `risk_flags`
- `warnings`

### 不允许承载的内容
- jerk / 插补模式 / 加速度
- 阀开关时间点
- PLC / 控制卡命令字
- 运行时实时偏移

### 与上下游关系
- 上游：`ProcessPlan`、`CoordinateTransformSet`、`AlignmentCompensation`
- 下游：`MotionPlan`

---

## 2.10 `MotionPlan`

### 对象职责
表示满足设备动态约束的可执行运动轨迹。

### 必填字段
- `id`
- `version`
- `artifact_type = MotionPlan`
- `job_id`
- `process_path_ref`
- `motion_segments`
- `dynamic_profile`
- `interpolation_modes`
- `motion_plan_result`
- `created_at`

### `motion_segments` 建议子字段
- `motion_segment_id`
- `process_segment_ref`
- `motion_type`
- `start_pose`
- `end_pose`
- `v_max`
- `a_max`
- `jerk_limit`
- `blend_policy`
- `sequence_index`

### 可选字段
- `lookahead_depth`
- `corner_tolerance`
- `lift_height`
- `time_estimate`
- `axis_group_id`
- `warnings`

### 不允许承载的内容
- 阀提前开 / 延迟关逻辑
- 材料配方状态
- 设备 ready 状态
- UI 显示字段

### 与上下游关系
- 上游：`ProcessPath`
- 下游：`DispenseTimingPlan`、`ExecutionPackage`

---

## 2.11 `DispenseTimingPlan`

### 对象职责
表示与运动段绑定的点胶工艺时序。

### 必填字段
- `id`
- `version`
- `artifact_type = DispenseTimingPlan`
- `job_id`
- `motion_plan_ref`
- `process_plan_ref`
- `timing_events`
- `timing_plan_result`
- `created_at`

### `timing_events` 建议子字段
- `event_id`
- `motion_segment_ref`
- `event_type`（`valve_on / valve_off / dwell / retract / purge`）
- `trigger_basis`
- `trigger_value`
- `channel_id`
- `sequence_index`

### 可选字段
- `pre_open_distance`
- `post_close_distance`
- `pre_open_delay_ms`
- `post_close_delay_ms`
- `retract_amount`
- `settling_time_ms`
- `warnings`

### 不允许承载的内容
- 修改运动几何
- 重新排序路径
- 设备门禁状态
- 运行时报警历史

### 与上下游关系
- 上游：`MotionPlan`、`ProcessPlan`
- 下游：`ExecutionPackage`

---

## 2.12 `ExecutionPackage`

### 对象职责
表示一次可下发、可离线校验、可执行的冻结任务包。

### 必填字段
- `id`
- `version`
- `artifact_type = ExecutionPackage`
- `job_id`
- `motion_plan_ref`
- `dispense_timing_plan_ref`
- `production_baseline_ref`
- `run_mode`
- `package_hash`
- `build_result`
- `validation_result`
- `created_at`

### 条件必填
- 当 `validation_result = passed` 时：`validated_at`、`validated_by`、`offline_rule_profile_ref` 必填。
- 当 `validation_result = rejected` 时：`failure_code`、`reject_reasons` 必填。
- 当 `run_mode` 含首件策略时：`first_article_policy` 必填，枚举为 `required / not_required / waivable`。

### 可选字段
- `alignment_snapshot_ref`
- `machine_profile_ref`
- `safety_requirements`
- `dry_run_flag`
- `first_article_policy`
- `estimated_cycle_time`
- `estimated_dispense_length`
- `estimated_material_usage`
- `validated_at`
- `validated_by`
- `offline_rule_profile_ref`
- `failure_code`
- `reject_reasons`
- `warnings`

### 不允许承载的内容
- 运行时设备门禁快照
- 运行时中途改包指令
- 历史 fault 汇总

### 与上下游关系
- 上游：`MotionPlan`、`DispenseTimingPlan`
- 下游：`MachineReadySnapshot`、`FirstArticleResult`、正式执行

---

## 2.13 `MachineReadySnapshot`

### 对象职责
表示某一时刻设备是否满足执行门禁。

### 必填字段
- `id`
- `version`
- `artifact_type = MachineReadySnapshot`
- `job_id`
- `execution_package_ref`
- `servo_ready`
- `homed`
- `safety_chain_ok`
- `recipe_valid`
- `machine_mode`
- `ready_result`（`passed / blocked`）
- `created_at`

### 条件必填
- 当 `ready_result = blocked` 时：`failure_code`、`blocking_reasons`、`recommended_action` 必填。

### 可选字段
- `mix_ready`
- `pressure_ok`
- `vacuum_ok`
- `temperature_ok`
- `workpiece_present`
- `door_closed`
- `alarm_bits`
- `io_snapshot`
- `failure_code`
- `blocking_reasons`
- `recommended_action`
- `warnings`

### 不允许承载的内容
- 重新生成轨迹
- 重新生成时序
- 修改工艺参数
- 修改执行包内容

### 与上下游关系
- 上游：`ExecutionPackage` + 实时设备状态
- 下游：`FirstArticleResult`、正式执行

---

## 2.14 `FirstArticleResult`

### 对象职责
表示预览、空跑、低速首件及质检后的放行结论。

### 必填字段
- `id`
- `version`
- `artifact_type = FirstArticleResult`
- `job_id`
- `execution_package_ref`
- `machine_ready_snapshot_ref`
- `run_mode`
- `result_status`（`passed / rejected / waived / not_required / aborted`）
- `inspection_mode`
- `created_at`

### 条件必填
- 当 `result_status = rejected` 时：
  - `failure_code`
  - `rollback_target`
  - `recommended_action`
  - `decided_by`
  必填。
- 当 `result_status = waived` 时：
  - `waive_reason`
  - `approval_ref`
  - `approved_by`
  - `approved_at`
  必填。
- 当 `result_status = passed` 时：
  - `decided_by` 必填；
  - 若为人工检验，则 `approval_ref` 必填。
- 当 `result_status = not_required` 时：
  - `not_required_reason`
  - `policy_ref`
  必填。

### 可选字段
- `quality_gate_result`
- `operator_decision`
- `observed_issues`
- `image_refs`
- `measurement_refs`
- `failure_code`
- `rollback_target`
- `recommended_action`
- `decided_by`
- `waive_reason`
- `approval_ref`
- `approved_by`
- `approved_at`
- `not_required_reason`
- `policy_ref`
- `warnings`

### 不允许承载的内容
- 直接改写 `ExecutionPackage`
- 直接改写工艺模板
- 直接改写对位补偿基线
- 作为长期追溯总记录替代 `ExecutionRecord`

### 与上下游关系
- 上游：`ExecutionPackage`、`MachineReadySnapshot`
- 下游：正式执行放行、回退决策、追溯归档

---

## 2.15 `ExecutionRecord`

### 对象职责
表示一次任务执行完成后的追溯总记录，回答“这次实际发生了什么”。

### 必填字段
- `id`
- `version`
- `artifact_type = ExecutionRecord`
- `job_id`
- `execution_package_ref`
- `workflow_final_state`
- `execution_final_state`
- `exec_start_at`
- `exec_end_at`
- `completed_segment_count`
- `fault_summary`
- `trace_links`
- `created_at`

### 条件必填
- 当 `workflow_final_state in (Completed, Aborted, Faulted)` 且进入归档时：`archived_at`、`archived_by` 必填。
- 当执行过程中存在 fault：`fault_events`、`fault_codes` 至少一类明细引用必填。

### 可选字段
- `execution_id`
- `first_article_result_ref`
- `machine_ready_snapshot_ref`
- `skipped_segments`
- `pause_count`
- `recover_count`
- `actual_cycle_time`
- `actual_material_usage`
- `final_alignment_snapshot_ref`
- `alarm_history_refs`
- `operator_actions`
- `quality_result`
- `fault_events`
- `fault_codes`
- `archived_at`
- `archived_by`

### 不允许承载的内容
- 回写上游对象版本
- 用追溯结果“修正规划事实”
- 跳过上游对象直接充当事实源

### 与上下游关系
- 上游：全链路对象引用与运行时事件流
- 下游：归档、复盘、审计查询

---

# 3. 对象之间的最小依赖链

建议固定如下：

```text
JobDefinition
 -> SourceDrawing
 -> CanonicalGeometry
 -> TopologyModel
 -> FeatureGraph
 -> ProcessPlan
 -> CoordinateTransformSet
 -> AlignmentCompensation
 -> ProcessPath
 -> MotionPlan
 -> DispenseTimingPlan
 -> ExecutionPackage
 -> MachineReadySnapshot
 -> FirstArticleResult
 -> ExecutionRecord
```

补充规则：
- `AlignmentCompensation` 不再允许被“隐式跳过”；无补偿任务也必须生成 `not_required` 版本。
- `ExecutionPackage` 分为“已组包”和“已校验通过”两个事实层面，但对象实体仍是同一个 `ExecutionPackage`，由 `build_result` 与 `validation_result` 区分。

---

# 4. 哪些对象变化会导致下游失效

## 4.1 一旦变化，必须判定下游失效

| 变化对象 | 必须失效的下游对象 |
|---|---|
| `SourceDrawing` | `CanonicalGeometry` 及其后全部对象 |
| `CanonicalGeometry` | `TopologyModel` 及其后全部对象 |
| `TopologyModel` | `FeatureGraph` 及其后全部对象 |
| `FeatureGraph` | `ProcessPlan` 及其后全部对象 |
| `ProcessPlan` | `CoordinateTransformSet`、`AlignmentCompensation`、`ProcessPath` 及其后全部对象 |
| `CoordinateTransformSet` | `AlignmentCompensation`、`ProcessPath` 及其后全部对象 |
| `AlignmentCompensation` | `ProcessPath` 及其后全部对象 |
| `ProcessPath` | `MotionPlan`、`DispenseTimingPlan`、`ExecutionPackage`、运行时对象 |
| `MotionPlan` | `DispenseTimingPlan`、`ExecutionPackage`、运行时对象 |
| `DispenseTimingPlan` | `ExecutionPackage`、运行时对象 |
| `ExecutionPackage` | `MachineReadySnapshot`、`FirstArticleResult`、`ExecutionRecord` |

## 4.2 设备快照类对象变化，不必回写规划失效

以下对象变化只影响执行门禁或追溯，不应污染规划对象：
- `MachineReadySnapshot`
- 运行时 fault
- 设备报警清零
- HMI 展示状态

## 4.3 回退时的 owner 执行矩阵

| 回退目标 | 需要标记 `superseded` 的对象 | 执行 owner |
|---|---|---|
| `ToCoordinatesResolved` | `AlignmentCompensation`、`ProcessPath`、`MotionPlan`、`DispenseTimingPlan`、`ExecutionPackage` | M5、M6、M7、M8 |
| `ToAligned` | `ProcessPath`、`MotionPlan`、`DispenseTimingPlan`、`ExecutionPackage` | M6、M7、M8 |
| `ToProcessPathReady` | `MotionPlan`、`DispenseTimingPlan`、`ExecutionPackage` | M7、M8 |
| `ToMotionPlanned` | `DispenseTimingPlan`、`ExecutionPackage` | M8 |
| `ToTimingPlanned` | `ExecutionPackage` | M8 |
| `ToPackageValidated` | 运行时对象 `MachineReadySnapshot`、`FirstArticleResult`、`ExecutionRecord` | M9、M10 |

---

# 5. 最容易发生对象污染的地方

## 5.1 `CanonicalGeometry` 被偷偷写入修复结果
应把修复结果写到 `TopologyModel`，不能覆盖几何事实。

## 5.2 `FeatureGraph` 被偷偷塞入速度与阀延时
工艺规则应进入 `ProcessPlan`，不能污染特征层。

## 5.3 `MotionPlan` 里直接埋开阀/关阀事件
阀时序只能进入 `DispenseTimingPlan`。

## 5.4 `ExecutionPackage` 在运行时被临时改写
运行时如需变更，必须重新组包并生成新版本；禁止在活动包上“偷偷热改”。

## 5.5 `ExecutionRecord` 反向“修正”上游对象
追溯对象只能记录发生过的事实，不能成为上游事实源。

---

# 6. 对后续模块设计最有价值的结论

## 6.1 最关键的不是对象多，而是“引用与版本链统一”
后续 s05 / s07 / s09 一律以 `artifact_ref` 结构为跨模块引用标准。

## 6.2 `AlignmentCompensation` 的 NoOp 语义必须成为正式规格
这项修订用于消除“无需对位但又必须引用补偿”的联调死角。

## 6.3 `FirstArticleResult` 的条件必填是首件门控闭环的基础
没有 `failure_code` / `rollback_target` / `approval_ref` 的首件对象，不得进入放行或回退链。

## 6.4 `superseded` 必须由 owner 执行
这条规则将直接约束 s05 的接口设计、s07 的回退状态机、s09 的验证方法。
