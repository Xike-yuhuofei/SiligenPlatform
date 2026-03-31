# DXF Authority Layout 全局策略 V1

- 状态：Proposed
- 日期：2026-03-31
- 当前分支：`docs/workflow/ARCH-202-architecture-review-closeout`
- 适用范围：`modules/dispense-packaging`、`modules/workflow`、`apps/runtime-gateway`、`apps/hmi-app`

## 1. 目的

冻结 DXF 胶点权威布局的项目级上位策略，统一以下两类已有设计取向的分派边界：

- `SPEC-001` 的“按每条应出胶几何线段执行首尾双锚定和段内均匀分布”
- `SPEC-002` 的“连续 dispense span 的权威胶点布局”

本文不替代 feature spec，也不直接规定实现细节。本文只回答一个更高层的问题：在什么前提下应当采用统一连续布局，什么前提下必须先做拓扑/工艺分派。

## 2. 问题定义

当前项目已经具备统一弧长布局内核，但仍缺少一份显式的上位策略来约束以下边界：

1. 何时应把输入视为一个连续 span
2. 何时必须切分为多个 span
3. 哪些点必须被当成硬锚点保留
4. 哪些顶点只是几何显示特征，不必强制落点
5. 异常、例外与辅助图元如何进入 authority layout

缺少这层策略时，系统容易因为 DXF 原始实体顺序或局部样例压力，在“按段双锚定”和“连续 span 全局采样”之间来回摇摆。

## 3. 总体原则

### 3.1 统一内核优先

线、圆弧、样条不应各自拥有一套独立的主布局算法。项目应保留统一的 authority layout 内核，统一使用真实弧长语义做距离求解。

### 3.2 分类必须发生在拓扑/工艺层

项目允许分类，但分类对象不是“矩形 DXF”“弧形 DXF”“样条 DXF”这类文件外观，而是归一化后的路径拓扑和工艺语义。

### 3.3 真值只允许来自 authority layout

`glue_points` 只能直接来自 authority layout。HMI、gateway、preview 组装层都不得重新采样、补点、去重或重排来决定真值。

### 3.4 拓扑正确性高于 DXF 书写顺序

DXF 原始实体顺序可以作为输入线索，但不能直接等价于最终工艺路径分组。若拓扑识别与原始书写顺序冲突，应优先服从归一化后的路径拓扑。

## 4. 三层模型

### 4.1 几何归一化层

职责：

- 从 DXF / process path 中恢复可点胶路径图
- 建立 contour、chain、shared vertex、branch、revisit、辅助图元等关系
- 为后续策略分派提供稳定的拓扑对象

输出要求：

- 不再直接把“原始实体顺序”当成工艺真值
- 能稳定识别独立轮廓、回访顶点和辅助图元

### 4.2 策略分派层

职责：

- 决定 span 如何切分
- 决定哪些点是硬锚点
- 决定哪些特征属于显式例外
- 决定哪些图元不得进入主 authority layout

这一层只做有限分派，不重新定义弧长布局数学。

### 4.3 统一布局内核

职责：

- 对每个归一化后的 span 做全局距离求解
- 对闭环做确定性相位选择
- 对线、弧、样条沿真实可见路径定位点位
- 产出 authority trigger points、preview `glue_points`、binding 与 spacing validation 的共享真值

## 5. 归一化后允许的拓扑类型

| 拓扑类型 | 定义 | 默认策略 |
|---|---|---|
| `open_chain` | 有明确起点终点的单一路径 | 全局等距采样，强制首尾锚定 |
| `closed_loop` | 首尾闭合的单一轮廓 | 全局等距采样，做确定性相位选择，不默认固定几何起点 |
| `branch_or_revisit` | 从已访问顶点引出新支路，或路径回访旧顶点后继续前进 | 默认切成独立 span，不得仅因实体连续就并为单一 open span |
| `multi_contour` | 多个彼此独立的 contour 或 chain | 每个 contour 独立布局，跨 contour 顺序由 planning 显式决定 |
| `auxiliary_geometry` | 参与显示或参考，但默认不属于点胶主轮廓的图元 | 默认不进入 authority layout |
| `exception_feature` | 短边、高曲率、小半径、用户指定关键点等需要显式说明的特征 | 允许例外，但必须带原因和边界 |

## 6. 锚点合同

### 6.1 硬锚点

以下点默认属于硬锚点：

- `open_chain` 的起点与终点
- 用户显式指定的工艺起终点
- 因 `branch_or_revisit` 切分后形成的新 span 起终点
- 短段或业务例外的边界点
- 明确要求必须保留的工艺关键点

硬锚点优先级高于 spacing 美观。

### 6.2 软顶点

以下点默认属于软顶点：

- 普通折线拐角
- 线弧切换点
- flatten 后的插值顶点
- 仅为显示存在的辅助顶点

软顶点默认不保证必然落点。

### 6.3 冲突处理

当硬锚点与固定 spacing 冲突时：

1. 先保留硬锚点
2. 允许 spacing deviation 进入显式 exception
3. 不允许为了“视觉均匀”隐式丢失硬锚点

## 7. 默认分派规则

### 7.1 `open_chain`

- 使用 span 级全局等距采样
- 必须强制首尾锚定
- 中间软顶点不默认强制落点

### 7.2 `closed_loop`

- 使用 span 级全局等距采样
- 在无硬锚点时通过确定性相位选择得到稳定布局
- 不得因几何实体的第一个顶点而固定布局起点

### 7.3 `branch_or_revisit`

- 一旦检测到回访旧顶点后继续前进，或从已访问顶点引出新支路，默认必须切分
- 切分后的每个子 span 再分别应用 `open_chain` 或 `closed_loop` 规则
- 不允许把“回到旧顶点后接出新线”仅视为一个连续 open span

### 7.4 `multi_contour`

- 各 contour 独立布局
- contour 之间的执行顺序、优先级和是否连喷，必须由 planning 显式定义
- 不得仅按 DXF 写入顺序推断多 contour 的工艺顺序

### 7.5 `auxiliary_geometry`

- 默认只保留在显示或诊断层
- 若要进入 authority layout，必须有明确的上游工艺语义提升

### 7.6 `exception_feature`

- 允许显式 deviation
- 必须输出例外原因、作用边界与是否阻断
- 例外不能退化成“遇到样例就打补丁”

## 8. 统一内核不变量

以下不变量在任何拓扑分派后都必须成立：

1. `glue_points` 逐点来自 authority layout，而不是 preview 层重建结果
2. `preview`、`spacing validation`、`execution binding` 必须共享同一份 authority points
3. 线按真实长度定位，弧按真实弧长定位，样条按误差受控可见路径定位
4. 几何等价但分段方式不同的输入，应给出等价布局
5. 同一有效输入与同一规划参数，authority layout 必须确定且可重复

## 9. 诊断与可追溯要求

authority layout 至少应能追溯以下信息：

- 当前 span 的拓扑类型
- span 的切分原因
- 当前 span 的硬锚点集合
- 当前 span 的相位策略或起止锚定策略
- 是否存在显式 spacing exception
- preview / binding / execution 是否共享同一 `layout_id`

没有这些诊断信息时，系统很难区分“算法错误”“拓扑误判”和“工艺例外”。

## 10. 与现有设计的对齐结论

### 10.1 对 `SPEC-001` 的对齐

`SPEC-001` 中“按每条应出胶几何线段执行首尾双锚定”的表述，不应被理解为所有连续路径一律按原始几何段独立布点。

项目级解释为：

- 当归一化结果把路径切分为多个 `open_chain`，或业务显式要求段边界为硬锚点时，允许应用双锚定
- 双锚定是分派结果，不是默认的全局唯一路径规则

### 10.2 对 `SPEC-002` 的对齐

`SPEC-002` 中“连续 dispense span 的权威胶点布局”保持为默认布局内核，但它不等于“凡是 DXF 实体连续，就必须并为一个 span”。

项目级解释为：

- `continuous-span-layout` 是每个归一化 span 的布局原点
- span 形成之前，必须先经过拓扑/工艺分派判断

## 11. 明确禁止项

以下行为在项目级策略上禁止：

1. 为矩形、圆形、弧形、样条分别维护互不一致的主布局算法
2. 把 DXF 原始实体顺序直接视为工艺布局真值
3. 让 HMI 或 gateway 通过显示层逻辑修正 authority `glue_points`
4. 在 authority 缺失时退回 `runtime_snapshot`、显示重采样结果或历史结果
5. 因单一样例问题引入不可解释的图形外观特例

## 12. 本版冻结结论

本版先冻结以下项目级结论：

1. 项目采用“统一弧长内核 + 拓扑/工艺策略分派”，不采用“按图形外观拆多套算法”
2. 分类对象是归一化后的路径拓扑，而不是 DXF 文件外观
3. `branch_or_revisit` 默认切分为独立 span
4. 硬锚点与软顶点必须显式区分
5. `glue_points` 只允许来自 authority layout，显示层不得补真值

## 13. 后续设计入口

后续若继续细化实现，应围绕以下问题展开，而不是从单个样例反推局部补丁：

1. 归一化层如何稳定识别 `branch_or_revisit` 与 `auxiliary_geometry`
2. 硬锚点集合如何进入 `AuthorityTriggerLayoutPlanner`
3. contour / span 切分结果如何进入 execution order 决策
4. exception 的阻断与非阻断边界如何统一输出
5. 现有 authority data model 是否需要补充拓扑分类与切分原因字段

## 14. 项目级实现决策表

以下决策表用于把本文件中的项目级原则映射为后续实现的最小落点。它不是代码设计细节，但定义了实现必须产出的决策元数据与责任边界。

| 冻结决策 | 输入判断 | Owner 层 | 强制输出 / 约束 | 首批落点 |
|---|---|---|---|---|
| D1: `span` 形成先于布局 | 共享顶点、回访旧顶点、branch、多 contour、辅助图元、显式工艺边界 | 几何归一化层 + 策略分派层 | 每个进入布局的 span 必须带 `topology_type`、`split_reason`；禁止把 DXF 原始实体顺序直接视为 span 真值 | `DispenseSpan`；`AuthorityTriggerLayoutPlanner` 输入 |
| D2: 硬锚点与软顶点显式分离 | open 起终点、业务关键点、切分边界点、短段例外边界、普通折线角点、线弧切换点 | 策略分派层 | 每个 span 必须输出 `strong_anchors` 或等价集合，以及 `anchor_policy`；普通几何顶点不得自动升格为强锚点 | `DispenseSpan`；`StrongAnchor`；planner 输入合同 |
| D3: 统一弧长内核不按图形外观切算法 | line / arc / spline 只是定位实现差异，不是主布局分叉点；open / closed 由拓扑决定 | `AuthorityTriggerLayoutPlanner` + `PathArcLengthLocator` | 每个 span 必须输出统一布局结果；闭环必须带 `phase_strategy`；开放路径固定起止锚定；禁止为矩形或弧形引入独立主算法 | `AuthorityTriggerLayout`；`DispenseSpan` |
| D4: 例外必须显式而非隐式补丁 | spacing 偏差、短段、闭环无法接受相位、曲线定位失败、binding 失败 | authority layout + validation + workflow gate | 例外必须输出 `validation_state`、`exception_reason` 或 `blocking_reason`；禁止用显示层修正掩盖例外 | `SpacingValidationOutcome`；`ExecutionLaunchGate` |
| D5: 真值链必须单源闭合 | authority points、preview payload、execution binding、confirm/start gate | facade + workflow use case + gateway/HMI consumer | 所有预览与执行链路必须共享同一 `layout_id` / `layout_ref`；`glue_points` 只允许直接投影自 authority points | `PreviewSnapshotPayload`；`ExecutionLaunchGate`；协议契约 |

## 15. 对当前样例的约束性解释

以 `rect_diag.dxf` 为例，本文件对实现层的约束不是“把矩形做特殊处理”，而是：

1. 归一化与分派阶段不得把“矩形四边 + 回到旧顶点后的对角线”仅因为实体连续就并成单一 `open_chain`
2. 该样例至少应被识别为“存在 `branch_or_revisit` 或独立 contour/独立 span 的组合”，再分别进入统一弧长内核
3. HMI 若看到角点分布异常，只能如实显示 authority `glue_points`，不能通过显示层重采样把异常抹平
