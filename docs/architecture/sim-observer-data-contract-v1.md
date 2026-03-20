# 数据契约规格说明 V1

## 1. 文档目标与适用范围

本文件定义模拟器可视化界面的统一数据契约，作为后续开发与验收的正式上游基线。

适用范围：

- `apps/hmi-app` 中的 `sim_observer` 功能域
- `结果摘要 / 结果画布 / 路径结构 / 上下文详情 / 时间过程 / 报警上下文` 六类视图
- 与这些视图相关的联动、回退、空态与失败保护

非目标：

- UI 技术选型
- 渲染实现细节
- 自动根因分析
- 自动工艺质量裁决

边界约束：

- 可视化宿主位于 `apps/hmi-app`
- `packages/simulation-engine` 继续作为仿真事实生产者，不吸入 UI 宿主职责
- 下游实现只能消费本文件定义的对象、锚点、状态与规则结果，不得自创平行模型

## 2. 权威输入面

可视化观察层的权威输入面固定为以下 recording/export 事实：

- `summary`
- `snapshots`
- `timeline`
- `trace`
- `motion_profile`

任何下游模块都不得重新发明第二套观察数据体系。

## 3. 统一对象身份模型

### 3.1 基础约束

- `ObjectId`：跨视图稳定唯一标识
- `ObjectType`：对象类别枚举
- `SourceRefs`：回指到原始 recording/export 事实的位置集合
- `DisplayLabel`：展示名，不替代对象身份

同一对象在摘要、画布、结构、详情、时间和报警视图中必须可互认，不允许每个视图维护一套本地对象身份。

### 3.2 统一对象类型

首批统一对象类型包括：

- `path_segment`
- `path_point`
- `region`
- `issue`
- `alert`
- `time_anchor`
- `object_group`

## 4. 对象模型

### 4.1 基础对象

- `PathSegmentObject`
  - 段序号
  - 段类型
  - 起点、终点
  - 空间包围盒
- `PathPointObject`
  - 所属段
  - 点序号
  - 点坐标
- `RegionObject`
  - 区域类型
  - 几何引用
  - 规则来源
  - 可选包围盒
- `IssueObject`
  - 问题类别
  - 问题级别
  - 可选主锚点类型
  - 可选问题状态
- `AlertObject`
  - 报警时间锚点
  - 可选窗口状态摘要
- `TimeAnchorObject`
  - 时间点或时间区间
  - 来源类型
- `ObjectGroup`
  - 成员对象集合
  - 集合来源
  - `GroupBasis`
  - 集合规模
  - 是否已收敛为最小关联集合

### 4.2 对象模型原则

- `ObjectGroup` 是独立对象，不是临时数组
- `PathPointObject` 默认只作为细粒度补充证据，不承担区域派生主成员职责
- 允许部分业务字段在 `V1` 保持 `str`，若后续跨模块漂移，再提升为枚举

## 5. 引用与锚点模型

### 5.1 锚点类型

- `ObjectAnchor`
- `RegionAnchor`
- `TimeAnchor`
- `AlertAnchor`

### 5.2 辅助引用

- `SequenceRangeRef`
  - 描述顺序连续性
  - 不替代空间映射
- `SpatialRef`
  - 描述对象或对象集合的空间映射结果

### 5.3 锚点规则

- 每条链路启动前都必须能判定“锚点足够 / 锚点不足”
- `ObjectAnchor(object_id + group_id)` 合法，语义为“主对象 + 所属集合上下文”
- 锚点不足时只能降级，不能补猜

## 6. 统一状态枚举

统一状态枚举如下：

- `resolved`
- `empty_but_explained`
- `mapping_missing`
- `mapping_insufficient`
- `group_not_minimized`
- `window_not_resolved`

这些状态是全局状态，不允许视图层写 magic string 或另起一套状态系统。

## 7. 统一失败语义

统一失败语义如下：

- `anchor_missing`
- `mapping_missing`
- `mapping_insufficient`
- `group_not_minimized`
- `window_not_resolved`
- `empty_state_reason`

说明：

- 状态回答“当前结果处于什么级别”
- 失败语义回答“为什么失败、降级或进入空态”

两者不是同一维度，不能混用。

## 8. 统一原因码

规则结果中的 `reason_code` 必须来自统一定义，不允许由视图或规则实现自由拼接。

首批至少覆盖以下原因码：

- `direct_object_anchor`
- `minimized_group_selected`
- `fallback_to_summary_only`
- `group_not_minimized`
- `group_minimized`
- `group_contains_removable_members`
- `no_direct_relation`
- `region_geometry_incomplete`
- `spatial_mapping_missing`
- `partial_spatial_mapping`
- `time_mapping_insufficient`
- `concurrent_candidates_present`
- `anchor_only_fallback`
- `window_not_resolved`
- `key_window_resolved`
- `sequence_range_resolved`
- `mapping_missing`

## 9. 核心规则集

### R1 主关联对象选择

目标：

- 为问题项、报警项、时间锚点等观察入口选择默认主落点

核心约束：

- 仅当单对象同时满足唯一性、稳定性、可解释性时，才允许优先于对象集合
- 主锚点唯一，其余锚点仅保留为上下文
- 证据不足时宁可退化为 `object_group` 或 `summary_only`，也不能伪造单对象

### R2 最小关联集合收敛

目标：

- 在无法可靠收敛到单对象时，得到最小关联集合

核心约束：

- 集合必须满足完整性、排他性、可解释性
- 若集合仍包含可删除且不影响当前判断的成员，则视为未收敛
- 若收敛为单对象，必须回交 `R1`

### R3 区域到对象集合派生

目标：

- 从 `RegionAnchor` 派生稳定、可解释的对象集合

核心约束：

- 默认以段为主，不以点为主
- 直接证据仅包括 `in_region / intersects_region / boundary_touch`
- 无直接关联对象但结论可靠时，状态归类为 `empty_but_explained`

### R4 结构到空间映射

目标：

- 把结构对象、对象组或顺序区间稳定回接到画布空间位置

核心约束：

- 结构顺序不能反向生成几何
- 部分映射不得静默丢弃
- `SequenceRangeRef` 只表达顺序连续性，不替代 `SpatialRef`

### R5 时间到对象或区间映射

目标：

- 把 `TimeAnchor` 映射到对象、对象集合或顺序区间

核心约束：

- 显式对象事实优先
- “可唯一确定”必须同时满足时间语义一致、对象身份稳定、且不存在同级并发候选
- 映射不足时必须停留在“过程事实已知、对象映射不足”

### R6 报警默认前后窗口范围

目标：

- 为报警分析提供默认上下文窗口

核心约束：

- 默认窗口围绕报警前提与直接后果组织
- `single_anchor_fallback` 不作为状态，而作为 `window_basis` 或 `fallback_mode`
- 上下文不足时不能伪装为完整窗口

### R7 关键窗口收敛

目标：

- 在默认窗口过大时收敛出关键窗口

核心约束：

- 关键窗口必须同时保留因果可解释性、去掉非必要噪音，并能支撑对象或区间回接
- 窗口无法收敛时必须显式停在 `window_not_resolved`

## 10. 不可接受状态

以下情况属于直接红线，不得在下游实现中出现：

- 无锚点伪定位
- 时间锚点伪对象高亮
- 报警缺上下文却展示完整因果链
- 结构无法映射空间却不暴露缺失
- 未收敛集合继续走单对象链路
- 多锚点冲突时强行合并为单一确定结论

## 11. 视图消费约束

### 11.1 摘要视图

- 只消费 `IssueObject` 的问题入口、问题级别、问题类别和下钻入口
- 不得自造对象归属

### 11.2 画布视图

- 只消费 `SpatialRef`、对象身份和异常状态
- 不得用顺序信息反推几何

### 11.3 结构视图

- 只消费结构对象、顺序关系和空间映射结果
- 不得自行猜测空间位置

### 11.4 详情视图

- 只消费当前锚点及其 `SourceRefs`、状态和原因码
- 不得膨胀为全局事实中心

### 11.5 时间视图

- 只消费 `TimeAnchor`、时间映射结果和关键窗口结果
- 不得在映射不足时假定对象

### 11.6 报警视图

- 只消费 `AlertAnchor`、上下文窗口和其回接结果
- 不得扩张为统一异常中心

## 12. 下游约束

下游所有实现文档默认带这一句：

> 本说明以《数据契约规格说明 V1》为唯一数据契约来源。

同时必须满足：

- `D2-D5` 禁止新增自定义对象身份模型
- `D2-D5` 禁止新增平行失败状态
- 所有视图联动、回退、空态和失败保护必须基于本文件契约
- 若下游发现契约不足，只能回提 `D1` 扩展，不得本地绕过

## 13. 当前代码落位

当前 `D1` 已在以下代码文件中形成第一批实现基线：

- `apps/hmi-app/src/hmi_client/features/sim_observer/contracts/observer_types.py`
- `apps/hmi-app/src/hmi_client/features/sim_observer/contracts/observer_status.py`
- `apps/hmi-app/src/hmi_client/features/sim_observer/contracts/observer_rules.py`
- `apps/hmi-app/src/hmi_client/features/sim_observer/adapters/recording_loader.py`
- `apps/hmi-app/src/hmi_client/features/sim_observer/adapters/recording_normalizer.py`

对应单元测试位于：

- `apps/hmi-app/tests/unit/sim_observer/`

## 14. 验收检查

本规格成立的最低检查项包括：

- 同一对象跨视图身份稳定
- 任一问题项都能明确落入 `single_object / object_group / summary_only`
- 任一区域、时间、报警入口都能明确落入统一状态枚举之一
- 失败时始终输出类型化失败语义，不输出伪对象、伪定位、伪因果
- 下游实现不需要额外发明平行对象模型或失败状态

## 15. 定版结论

本文件定版后，`D1` 视为模拟器可视化界面首批开发的正式数据基线。

后续不再继续做方向澄清，而是按本规格推进：

- `D2-D5` 开发落地
- `D6` 行为级验收
- 联调、收敛和发布决策
