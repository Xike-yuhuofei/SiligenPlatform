# DXF 输入治理冻结补充标准 v1

Status: Frozen Supplement Standard
Authority: Supplement to S01/S02/S04/S05/S09
Applies To: DXF input governance across S1-S5, S8, S11B, S12, S16
Owns / Covers: DXF validation, canonicalization, topology quality, layer semantics, input quality gate, diagnostics reports, repair boundary
Related Owners: M1 job-ingest / M2 dxf-geometry / M3 topology-feature / M4 process-planning / M6 process-path / M8 dispense-packaging / M9 runtime-execution / M10 trace-diagnostics
Must Not Override: S01-S10 formal spec axes; S04 object chain; S05 owner boundaries
Read When: handling DXF import, validation failure, geometry quality, topology rejection, layer semantics, path quality preconditions, input traceability
Conflict Priority: use this document for DXF governance detail only; defer object/owner/stage conflicts to S01-S10
Codex Keywords: DXF, SourceDrawing, CanonicalGeometry, TopologyModel, FeatureGraph, input_valid, DXFValidationReport, GeometryQualityReport, quality gate
Scope: 无痕内衣视觉点胶机上位机 DXF 输入治理
Target: 产品级可靠 + 算法领先基础版，并保留 L5 工艺闭环演进路径
Last Updated: 2026-04-24

---

## Codex Decision Summary

- 本文细化 DXF 输入治理、验证报告、质量门禁、诊断挂载和拒绝/修复边界。
- 本文不另起正式对象链，不改变 S1-S4 owner，不替代 S01-S10。
- DXF 不合格不得执行；禁止静默修复；所有报告必须挂载到正式对象诊断或追溯链。

---
# 1. 文档定位

本文档是 `docs/architecture/dsp-e2e-spec/` 正式冻结文档集下的 **DXF 输入治理专项冻结补充标准**。

本文档只作为冻结补充标准使用，不替代 `S01-S10`，不另建并行对象链。
凡涉及 DXF 输入治理的专项规则，必须回链到 `dsp-e2e-spec` 已冻结的阶段链、对象链、模块边界、状态机、测试矩阵与归档规则。

本文档主要细化以下阶段：

| 阶段 | 对象 | 本文档覆盖内容 |
|---|---|---|
| S1 | `SourceDrawing` | 文件接收、封存、hash、格式识别、文件级拒绝 |
| S2 | `CanonicalGeometry` | DXF 解析、单位统一、实体支持范围、几何标准化 |
| S3 | `TopologyModel` | 几何质量、拓扑重建、断点/自交/重复/开闭环治理 |
| S4 | `FeatureGraph` | 图层语义、制造特征识别、未分类特征阻断 |
| S5 | `ProcessPlan` | 工艺规则映射前置输入约束 |
| S8 | `ProcessPath` | 路径质量门禁前置约束 |
| S11B | `ExecutionPackage(validation_result=passed/rejected)` | 离线校验对 DXF 输入链结果的引用 |
| S12 | `MachineReadySnapshot` | 执行前门禁消费 `input_valid` / `path_quality_pass` |
| S16 | `ExecutionRecord` | DXF hash、验证报告、质量报告、失败阶段的追溯 |

---

# 2. 与 dsp-e2e-spec 的关系

## 2.1 本文档不另起正式对象链

DXF 输入治理必须嵌入现有正式对象链：

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

本文档中出现的 `DXFInputValidator`、`DXFValidationPolicy`、`DXFCanonicalizer`、`ContourTopologyAnalyzer`、`GeometryTolerancePolicy` 等名称均为 **服务 / policy / 内部组件**，不是独立于上述对象链之外的新 owner 对象。

## 2.2 本文档不改变 S1-S4 的 owner

| 阶段 | owner | 本文档允许细化 | 本文档禁止改写 |
|---|---|---|---|
| S1 | M1 | 文件级校验、封存、hash、格式拒绝 | 不得在 S1 解析几何 |
| S2 | M2 | DXF 解析、实体标准化、单位统一 | 不得在 S2 做拓扑修复、工艺分类、路径生成 |
| S3 | M3 | 拓扑重建、制造导向几何质量检查 | 不得输出工艺参数、轨迹、设备命令 |
| S4 | M3 | 制造特征识别、图层语义到特征的初步映射 | 不得输出速度、阀延时、执行段 |
| S5 | M4 | 工艺规则映射时消费 FeatureGraph | 不得反向修改 SourceDrawing / CanonicalGeometry |

## 2.3 Report 与正式对象的关系

本文档定义的报告不是正式对象链替代品。

| Report | 挂载位置 | 说明 |
|---|---|---|
| `DXFValidationReport` | `SourceDrawing.diagnostics` / `CanonicalGeometry.diagnostics` | 文件级、实体级、单位、图层、解析合法性报告 |
| `GeometryQualityReport` | `CanonicalGeometry.diagnostics` / `TopologyModel.diagnostics` | 几何退化、短段、重复、重叠、异常圆弧等 |
| `TopologyQualityReport` | `TopologyModel.diagnostics` | 开闭环、断点、自交、孤立线、悬挂边等 |
| `LayerSemanticReport` | `FeatureGraph.diagnostics` | 图层语义、未知图层、点胶图层缺失、语义冲突等 |
| `PathQualityReport` | `ProcessPath.diagnostics` / 后续质量门禁 | 点距、重复点、小折返、异常空移、越界等 |

---

# 3. 总原则

## 3.1 DXF 不是可直接执行文件

DXF 在本系统中不是“能打开就能点胶”的图纸文件。
DXF 必须先经过输入规范校验、解析标准化、拓扑质量检查、制造特征识别、工艺语义映射、路径质量门禁、预览执行一致性验证和执行安全门禁。

## 3.2 第一阶段不追求无限兼容

第一阶段目标不是兼容所有 CAD 软件、所有 DXF 版本、所有 DXF entity。
第一阶段目标是：

```text
稳定导入受控 DXF；
拒绝不合格 DXF；
报告明确原因；
问题可定位；
结果可追溯；
样例可回归。
```

## 3.3 质量门禁优先于执行成功率

必须坚持：

```text
DXF 不合格，不执行。
图层语义不明确，不执行。
轮廓拓扑不可靠，不执行。
路径质量不过关，不执行。
预览与执行语义不一致，不执行。
设备状态不安全，不执行。
```

核心原则：

```text
宁可拒绝执行，也不能错误执行。
```

## 3.4 禁止静默修复

禁止在未记录、未报告、未重新验证的情况下自动修改 DXF 语义。

禁止：

```text
静默连接断点
静默删除重复线
静默展开 block
静默采样 spline
静默假定单位为 mm
静默忽略未知点胶图层
静默把不可解释几何送入路径生成
```

允许：

```text
检测问题
报告问题
提出修复建议
生成修复预览
人工确认修复
记录修复行为
修复后重新验证
```

---

# 4. 终态定义

DXF 输入治理的终态是：

```text
DXF 从一个不可信 CAD 文件，
变成一个可验证、可拒绝、可解释、可定位、可追溯、可回归、可沉淀工艺知识的工程输入契约。
```

终态系统必须做到：

```text
同一个 SourceDrawing
+ 同一个 DXFInputSpec 版本
+ 同一个 DXFValidationPolicy 版本
+ 同一个算法版本
+ 同一个 GeometryTolerancePolicy 版本

必须得到稳定一致的 CanonicalGeometry、TopologyModel、FeatureGraph、验证报告和门禁结果。
```

---

# 5. 成熟度路线

## 5.1 Phase A：受控输入可靠化

目标成熟度：DXF 输入治理 L3。

覆盖阶段：S1 / S2 / S3 初步检查。

目标：

```text
只接受受控 DXF；
非法 DXF 稳定拒绝；
错误可定位；
报告可追溯；
fixture 可回归。
```

核心产物：

```text
DXFInputSpec
DXFValidationPolicy
DXFValidationReport
DXFValidationErrorCode
GoldenFixtures
PathologicalFixtures
```

验收标准：

```text
golden DXF 稳定通过；
pathological DXF 稳定拒绝；
错误报告包含 error_code、layer、entity、location、operator_message；
DXF invalid 不得进入 S5 之后。
```

## 5.2 Phase B：几何 / 拓扑制造语义可靠化

目标成熟度：DXF 输入治理 L4-，几何拓扑处理 L3-L4。

覆盖阶段：S2 / S3 / S4。

目标：

```text
系统不只读 DXF，而是能理解制造拓扑。
```

新增能力：

```text
闭合轮廓识别
开放胶线识别
孤立线识别
微小断点识别
重复线识别
重叠线识别
自交识别
内外轮廓关系识别
轮廓方向治理
制造特征分类
```

核心产物：

```text
TopologyModel
FeatureGraph
GeometryQualityReport
TopologyQualityReport
FeatureClassificationReport
GeometryTolerancePolicy
```

验收标准：

```text
解析成功但拓扑不可制造时，必须阻断在 S3；
拓扑成功但特征不可解释时，必须阻断在 S4；
不得把拓扑问题丢给 S5/S8/S9 兜底。
```

## 5.3 Phase C：输入治理与工艺规划闭环

目标成熟度：DXF 输入治理 L4，工艺建模 L3，路径质量 L3-L4。

覆盖阶段：S4 / S5 / S8。

目标：

```text
DXF 图层、拓扑和制造特征能够稳定映射为工艺对象。
```

新增能力：

```text
图层语义到制造特征映射
FeatureGraph 到 ProcessPlan 映射
点胶区 / 禁胶区 / 对位点 / 参考线分离
工艺参数绑定检查
路径覆盖率检查
未覆盖特征阻断
```

核心产物：

```text
LayerSemanticPolicy
FeatureGraph
ProcessPlan
ProcessPath
PathQualityReport
PathQualityGate
```

验收标准：

```text
未知图层不得静默进入工艺规划；
未分类 Feature 不得进入 ProcessPlan；
未覆盖 Feature 不得进入 ProcessPath；
PathQualityGate FAIL 不得进入 MotionPlan / ExecutionPackage。
```

## 5.4 Phase D：预览执行一致性与执行包校验

目标成熟度：预览执行一致性 L4，设备执行安全 L4。

覆盖阶段：S8 / S9 / S10 / S11A / S11B / S12。

目标：

```text
预览显示的不是 DXF 原图，而是执行前真值；
执行包未通过离线校验，不得进入设备预检。
```

新增能力：

```text
ExecutionPreviewModel
PreviewQualityOverlay
PreviewExecutionConsistencyCheck
ExecutionPackage offline validation
MachineReadySnapshot input_valid 检查
PreflightBlocked 不污染规划链
```

验收标准：

```text
预览与 execution package 的段语义一致；
preview snapshot 能回链 SourceDrawing / CanonicalGeometry / ProcessPath / MotionPlan；
ExecutionPackageValidated 前不得 StartExecution；
PreflightBlocked 不得导致 SourceDrawing / CanonicalGeometry / ProcessPlan / MotionPlan 失效。
```

## 5.5 Phase E：受控修复与版本化回退

目标成熟度：DXF 输入治理 L4+，追溯 L4。

覆盖阶段：S2 / S3 / S4 / S16。

目标：

```text
系统允许修复，但所有修复必须显式、可审计、可回滚、可重新验证。
```

新增能力：

```text
RepairProposal
RepairAction
RepairDiffPreview
RepairAuditRecord
ArtifactSuperseded
Revalidation
```

验收标准：

```text
任何修复不得覆盖原始 SourceDrawing；
CanonicalGeometry 变化必须导致 TopologyModel 及后续对象失效；
TopologyModel 变化必须导致 FeatureGraph 及后续对象失效；
修复后必须重新走 S2/S3/S4 验证。
```

## 5.6 Phase F：工艺数据闭环与输入质量智能化

目标成熟度：DXF 输入治理 L5，工艺闭环 L5。

覆盖阶段：S16 / trace-diagnostics / process knowledge base。

目标：

```text
DXF 输入质量不只是一次性门禁，
而是进入工艺知识库，
为款式、布料、胶水、点胶策略优化服务。
```

新增能力：

```text
DXFQualityHistory
CADSourceQualityProfile
StyleGeometryProfile
MaterialSensitiveGeometryRules
GlueStrategyRecommendation
HistoricalRepairPattern
InputQualityTrend
```

系统最终应能回答：

```text
哪类款式最容易出现断点？
哪类图层命名最容易误导工艺？
哪类 spline 转换导致点距异常？
哪类布料对小折返更敏感？
某个 CAD 输出流程是否稳定？
某类 DXF 是否应该升级输入规范？
```

验收标准：

```text
每次任务能关联 SourceDrawing hash、DXFValidationReport、TopologyQualityReport、PathQualityReport、ExecutionRecord；
能统计输入问题与首件失败 / 返工 / 路径质量异常之间的关系；
能形成下一版 DXFInputSpec / LayerSemanticPolicy / GeometryTolerancePolicy 的数据依据。
```

---

# 6. 标准输入范围 v1

## 6.1 文件级要求

第一阶段只接受受控 DXF。

必须满足：

```text
2D DXF
单位为 mm
坐标位于 XY 平面
Z = 0 或在 tolerance 内接近 0
文件非空
HEADER 可解析
ENTITIES 可解析
坐标值有限
无 NaN
无 Inf
```

不得包含：

```text
3D entity
未展开 block / insert
复杂 hatch
不明 spline
不受控图层
无单位声明
异常大坐标
空点胶层
```

## 6.2 推荐 DXF 版本策略

第一阶段推荐输入策略：

```text
优先使用 R12 / ASCII DXF 的受控子集；
只允许 LINE / ARC / CIRCLE / POLYLINE / LWPOLYLINE；
所有复杂对象在 CAD 或预处理工具中转换为受控几何。
```

说明：

```text
选择较窄 DXF 子集不是因为它先进，
而是因为它更容易形成稳定、可验证、可追溯的工业输入契约。
```

## 6.3 支持 entity

第一阶段允许：

```text
LINE
ARC
CIRCLE
POLYLINE
LWPOLYLINE
POINT
```

其中：

```text
POINT 只允许用于对位点、标记点或显式胶点；
POINT 不得在未定义图层语义时直接解释为点胶点。
```

## 6.4 谨慎支持 entity

以下 entity 不得默认进入执行链路：

```text
SPLINE
ELLIPSE
```

处理策略：

```text
默认拒绝；
如后续支持，必须先转换为 CanonicalGeometry 中的可验证 primitive；
必须记录拟合误差；
必须保留 source entity；
必须经过曲线误差验证；
必须进入回归测试。
```

## 6.5 禁止 entity

第一阶段禁止：

```text
INSERT
BLOCK 未展开内容
HATCH
TEXT
MTEXT
DIMENSION
LEADER
IMAGE
REGION
3DFACE
SOLID
MESH
SURFACE
BODY
ACAD_PROXY_ENTITY
```

默认处理：

```text
只要出现在执行相关图层，必须 FAIL。
出现在参考图层时，默认 WARNING 或 FAIL，由 policy 明确控制。
```

---

# 7. 图层语义规范

## 7.1 图层是工艺语义边界

DXF 图层不是装饰属性。
图层用于表达：

```text
哪些几何参与点胶
哪些几何用于视觉对位
哪些几何用于避让
哪些几何只是参考
哪些几何必须忽略
```

## 7.2 标准图层

第一阶段标准图层如下：

| 图层名 | 语义 | 是否生成点胶 | 是否参与安全检查 |
|---|---|---:|---:|
| GLUE_CONTOUR | 点胶轮廓 | 是 | 是 |
| GLUE_DOT | 显式胶点 | 是 | 是 |
| ALIGN_MARK | 视觉对位标记 | 否 | 是 |
| KEEP_OUT | 禁止区域 / 避让区 | 否 | 是 |
| CUT_LINE | 裁切线 | 默认否 | 可配置 |
| REFERENCE | 参考线 | 否 | 否 |
| IGNORE | 显式忽略 | 否 | 否 |

## 7.3 未知图层策略

未知图层不得静默解释。

默认策略：

```text
未知图层包含几何：FAIL
未知图层为空：WARNING
未知图层只含 TEXT / DIMENSION：WARNING 或 FAIL，由 policy 控制
未知图层疑似点胶几何：FAIL
```

## 7.4 图层语义禁止项

禁止：

```text
根据颜色猜测点胶语义
根据线型猜测点胶语义
根据图层顺序猜测点胶语义
根据文件名猜测点胶语义
根据 entity 数量猜测点胶语义
```

允许：

```text
通过明确配置的 LayerSemanticPolicy 映射图层语义。
```

---

# 8. 输入治理数据流

标准数据流必须回链到正式阶段对象：

```text
S1 SourceDrawing
  - 文件接收
  - 文件封存
  - file_hash
  - 文件级格式校验
  - SourceFileAccepted / SourceFileRejected

S2 CanonicalGeometry
  - DXF 解析
  - 单位统一
  - entity 支持范围校验
  - 坐标标准化
  - CanonicalGeometryBuilt / CanonicalGeometryRejected

S3 TopologyModel
  - 几何质量分析
  - 拓扑重建
  - 断点 / 重复 / 自交 / 开闭环检查
  - TopologyBuilt / TopologyRejected

S4 FeatureGraph
  - 图层语义识别
  - 制造特征提取
  - 未分类 / 冲突特征阻断
  - FeatureGraphBuilt / FeatureClassificationFailed
```

内部服务可按如下组织，但不得替代正式对象链：

```text
DXFReader
DXFInputValidator
DXFCanonicalizer
ContourTopologyAnalyzer
FeatureClassifier
DXFValidationPolicy
GeometryTolerancePolicy
LayerSemanticPolicy
```

---

# 9. 职责边界

## 9.1 M1 / S1：SourceDrawing

职责：

```text
接收 DXF 原件
封存原始文件
生成 file_hash
识别文件格式
记录文件元信息
输出 SourceDrawing
```

不得：

```text
解析几何
修复几何
生成 CanonicalGeometry
生成拓扑
生成点胶路径
决定设备是否执行
```

## 9.2 M2 / S2：CanonicalGeometry

职责：

```text
解析 DXF header / tables / entities
统一单位
统一坐标表达
识别 unsupported entity
输出 CanonicalGeometry
生成 DXFValidationReport / GeometryQualityReport 的解析部分
```

不得：

```text
做工艺分类
做拓扑修复
生成 FeatureGraph
生成 ProcessPlan
生成 ProcessPath
调用 runtime / 设备 adapter
```

## 9.3 M3 / S3：TopologyModel

职责：

```text
检查几何退化
检查重复线 / 重叠线 / 极短线段
识别开闭环
识别断点
识别孤立线
识别自交
建立 TopologyModel
```

不得：

```text
生成工艺参数
生成路径速度
生成阀时序
直接生成 ExecutionPackage
```

## 9.4 M3 / S4：FeatureGraph

职责：

```text
根据 TopologyModel 与 LayerSemanticPolicy 提取制造特征
识别点胶轮廓 / 显式胶点 / 对位点 / 禁止区域 / 参考线
输出 FeatureGraph
记录未分类特征与冲突特征
```

不得：

```text
直接生成 MotionPlan
直接生成 DispenseTimingPlan
直接调用设备
```

## 9.5 M4 / S5：ProcessPlan

职责：

```text
消费 FeatureGraph
绑定工艺模板、材料、胶水、针嘴、点距、速度策略
生成 ProcessPlan
```

不得：

```text
反向修改 SourceDrawing
反向修改 CanonicalGeometry
静默忽略未分类 Feature
```

---

# 10. CanonicalGeometry 要求

## 10.1 目的

`CanonicalGeometry` 是 DXF 输入治理后的几何事实对象。
它不是胶点，不是运动轨迹，不是执行段。

它用于连接：

```text
SourceDrawing
几何拓扑分析
制造特征识别
工艺路径生成
问题定位
追溯复现
```

## 10.2 建议 primitive 类型

第一阶段建议定义：

```text
CanonicalLine
CanonicalArc
CanonicalCircle
CanonicalPolyline
CanonicalPoint
```

后续可扩展：

```text
CanonicalSplineApproximation
CanonicalEllipseApproximation
```

但扩展必须满足：

```text
可验证
可追溯
有拟合误差
有 fixture
有回归测试
```

## 10.3 Primitive 必须字段

每个 primitive 建议包含：

```text
primitive_id
source_drawing_ref
source_hash
source_entity_id
source_entity_type
source_layer
source_color
source_handle
geometry_type
start_point
end_point
bbox
length_mm
is_closed
is_2d
z_range
approximation_error_mm
metadata
```

## 10.4 禁止过早点流化

禁止在 S1/S2 阶段直接把所有几何转换为点列。

原因：

```text
点列会丢失 entity 来源；
点列会丢失圆弧语义；
点列会丢失图层语义；
点列会掩盖断点、重复线、自交等拓扑问题；
点列会让错误无法定位回 CAD 实体。
```

允许点流化的位置：

```text
S8 ProcessPath 生成后
路径质量分析时
ExecutionPreviewModel 生成时
S9/S10/S11 执行前模型生成时
```

但必须能追溯到 `CanonicalGeometry` 和 `SourceDrawing`。

---

# 11. 验证项目

## 11.1 文件级验证

必须检查：

```text
文件存在
文件非空
文件大小合理
DXF header 可读
DXF section 可读
单位明确
单位为 mm
DXF 版本符合 policy
```

错误码示例：

```text
DXF_FILE_NOT_FOUND
DXF_FILE_EMPTY
DXF_FILE_TOO_LARGE
DXF_HEADER_UNREADABLE
DXF_SECTION_UNREADABLE
DXF_UNIT_MISSING
DXF_UNIT_NOT_MM
DXF_VERSION_UNSUPPORTED
```

## 11.2 坐标级验证

必须检查：

```text
所有坐标有限
无 NaN
无 Inf
Z 值为 0 或在 tolerance 内
坐标范围不超过机器配置允许范围
bbox 可计算
```

错误码示例：

```text
DXF_COORDINATE_NAN
DXF_COORDINATE_INF
DXF_3D_ENTITY_DETECTED
DXF_Z_OUT_OF_TOLERANCE
DXF_COORDINATE_OUT_OF_RANGE
DXF_BOUNDS_UNAVAILABLE
```

## 11.3 Entity 支持范围验证

必须检查：

```text
entity 类型是否受支持
是否存在 block / insert
是否存在 hatch
是否存在 spline
是否存在 ellipse
是否存在 proxy entity
是否存在执行相关图层上的非执行 entity
```

错误码示例：

```text
DXF_UNSUPPORTED_ENTITY
DXF_BLOCK_INSERT_NOT_EXPLODED
DXF_HATCH_NOT_SUPPORTED
DXF_SPLINE_NOT_SUPPORTED
DXF_ELLIPSE_NOT_SUPPORTED
DXF_PROXY_ENTITY_NOT_SUPPORTED
DXF_EXECUTION_LAYER_HAS_NON_EXEC_ENTITY
```

## 11.4 图层语义验证

必须检查：

```text
是否存在未知图层
是否存在空点胶图层
是否存在多个冲突语义图层
是否缺少必要点胶图层
是否图层名大小写不一致
是否图层语义未配置
```

错误码示例：

```text
DXF_UNKNOWN_LAYER
DXF_EMPTY_GLUE_LAYER
DXF_MISSING_GLUE_LAYER
DXF_LAYER_SEMANTIC_CONFLICT
DXF_LAYER_CASE_MISMATCH
DXF_LAYER_POLICY_MISSING
```

## 11.5 几何质量验证

必须检查：

```text
零长度 entity
极短线段
重复实体
重叠实体
异常小圆
异常小圆弧
圆弧半径非法
圆弧角度非法
polyline 点数量不足
polyline 自重复点
```

错误码示例：

```text
GEOM_ZERO_LENGTH_ENTITY
GEOM_TINY_SEGMENT
GEOM_DUPLICATED_ENTITY
GEOM_OVERLAPPING_ENTITY
GEOM_TINY_CIRCLE
GEOM_TINY_ARC
GEOM_INVALID_ARC_RADIUS
GEOM_INVALID_ARC_ANGLE
GEOM_POLYLINE_TOO_FEW_POINTS
GEOM_POLYLINE_DUPLICATED_VERTEX
```

## 11.6 拓扑质量验证

必须检查：

```text
开放轮廓
断点
孤立线
悬挂边
自交
重复轮廓
异常小环
轮廓方向异常
多轮廓关系异常
```

错误码示例：

```text
TOPOLOGY_OPEN_CONTOUR
TOPOLOGY_GAP_TOO_LARGE
TOPOLOGY_ISOLATED_ENTITY
TOPOLOGY_DANGLING_EDGE
TOPOLOGY_SELF_INTERSECTION
TOPOLOGY_DUPLICATED_CONTOUR
TOPOLOGY_TINY_LOOP
TOPOLOGY_DIRECTION_INCONSISTENT
TOPOLOGY_CONTOUR_RELATION_INVALID
```

## 11.7 工艺语义验证

必须检查：

```text
是否存在可点胶图层
点胶轮廓是否可解释为 FeatureGraph
显式胶点是否有合法语义
是否存在禁止区域冲突
是否缺少对位标记
是否超出工作空间
是否缺少工艺参数绑定
```

错误码示例：

```text
PROCESS_NO_GLUE_GEOMETRY
PROCESS_GLUE_CONTOUR_NOT_INTERPRETABLE
PROCESS_GLUE_DOT_INVALID
PROCESS_KEEP_OUT_INTERSECTION
PROCESS_ALIGNMENT_MARK_MISSING
PROCESS_WORKSPACE_BOUNDS_EXCEEDED
PROCESS_RECIPE_MISSING
```

---

# 12. Validation Report

## 12.1 DXFValidationReport 是必须产物

每次 DXF 导入必须生成 `DXFValidationReport`，并挂到对应阶段对象的 diagnostics 中。

不允许只返回：

```text
导入成功
导入失败
```

必须返回结构化报告。

## 12.2 Report 必须字段

```json
{
  "schema_version": "DXFValidationReport.v1",
  "file": {
    "file_name": "",
    "file_hash": "",
    "file_size_bytes": 0,
    "dxf_version": "",
    "unit": "mm",
    "source_drawing_ref": null
  },
  "policy": {
    "spec_version": "DXFInputSpec.v1",
    "policy_version": "DXFValidationPolicy.v1",
    "tolerance_policy_version": "GeometryTolerancePolicy.v1"
  },
  "stage": {
    "stage_id": "S1|S2|S3|S4",
    "owner_module": "M1|M2|M3"
  },
  "summary": {
    "gate_result": "PASS | PASS_WITH_WARNINGS | FAIL",
    "error_count": 0,
    "warning_count": 0,
    "entity_count": 0,
    "layer_count": 0,
    "glue_contour_count": 0,
    "bounds_mm": {
      "min_x": 0,
      "min_y": 0,
      "max_x": 0,
      "max_y": 0
    }
  },
  "entity_summary": [],
  "layer_summary": [],
  "geometry_summary": {},
  "topology_summary": {},
  "errors": [],
  "warnings": [],
  "recommended_actions": []
}
```

## 12.3 Error 必须字段

```json
{
  "error_code": "TOPOLOGY_OPEN_CONTOUR",
  "severity": "ERROR",
  "message": "",
  "operator_message": "",
  "stage_id": "S3",
  "owner_module": "M3",
  "layer": "",
  "entity_id": "",
  "entity_type": "",
  "source_handle": "",
  "location_mm": {
    "x": 0,
    "y": 0
  },
  "related_entities": [],
  "suggested_fix": "",
  "is_blocking": true
}
```

## 12.4 Severity 定义

```text
INFO
WARNING
ERROR
FATAL
```

语义：

```text
INFO：不影响执行，仅用于说明。
WARNING：存在风险，但 policy 允许继续。
ERROR：阻断当前阶段，不允许进入后续执行链路。
FATAL：文件或解析状态不可用，必须终止导入。
```

## 12.5 Gate Result 定义

```text
PASS
PASS_WITH_WARNINGS
FAIL
```

规则：

```text
存在 FATAL => FAIL
存在 blocking ERROR => FAIL
仅存在 WARNING => PASS_WITH_WARNINGS
无 ERROR / WARNING => PASS
```

禁止：

```text
将 ERROR 静默降级为 WARNING
通过 HMI 操作绕过 FAIL
通过 runtime 参数绕过 DXF 输入治理
```

---

# 13. 默认 tolerance 策略

## 13.1 tolerance 必须集中定义

不得在各模块散落 magic number。
必须由 `GeometryTolerancePolicy` 提供。

## 13.2 第一阶段建议默认值

以下值为初始建议，必须可配置，并随机器精度、胶点直径、视觉误差校准：

```text
position_tolerance_mm = 0.02 ~ 0.05
z_tolerance_mm = 0.01
duplicate_tolerance_mm = 0.02
gap_tolerance_mm = 0.05 ~ 0.20
min_segment_length_mm = 0.10 ~ 0.30
min_contour_length_mm = 1.00
arc_fit_error_limit_mm = 0.03 ~ 0.10
workspace_margin_mm = machine_config.controlled
```

## 13.3 tolerance 使用规则

必须：

```text
同一任务使用同一套 tolerance snapshot；
report 中记录 tolerance policy 版本；
质量问题报告中显示实际值与阈值；
test fixture 中固定期望阈值。
```

禁止：

```text
validator 使用一套 tolerance；
topology analyzer 使用另一套未记录 tolerance；
path quality analyzer 使用第三套未记录 tolerance。
```

---

# 14. 阶段阻断规则

## 14.1 阻断位置

DXF 输入治理阻断必须发生在对应正式阶段：

| 阻断类型 | 阶段 | 事件 |
|---|---|---|
| 文件损坏 / 空文件 / hash 失败 | S1 | `SourceFileRejected` |
| 单位不明 / unsupported entity / 解析失败 | S2 | `CanonicalGeometryRejected` |
| 断点 / 自交 / 拓扑不可制造 | S3 | `TopologyRejected` |
| 图层语义冲突 / 未分类关键特征 | S4 | `FeatureClassificationFailed` |
| 工艺模板缺失 / 参数不可用 | S5 | `ProcessRuleRejected` |
| 路径质量不合格 | S8 / path quality gate | `ProcessPathRejected` 或质量门禁失败 |
| 执行包离线校验失败 | S11B | `OfflineValidationFailed` |
| 设备未 ready | S12 | `PreflightBlocked` |

## 14.2 阻断条件

以下情况必须阻断：

```text
单位缺失
单位非 mm
3D entity
未展开 block / insert
unsupported entity 出现在执行相关图层
复杂 hatch
不支持 spline
未知点胶图层
缺少点胶图层
开放轮廓超过阈值
断点超过阈值
重复线严重
自交轮廓
工作空间越界
缺少必要工艺参数
```

## 14.3 可警告条件

以下情况可作为 WARNING，但必须由 policy 明确允许：

```text
非执行图层存在 TEXT
参考图层存在尺寸标注
图层命名大小写不推荐
少量非执行图层空对象
轮廓方向可自动统一
轻微非阻断几何噪声
```

---

# 15. HMI 呈现要求

## 15.1 HMI 必须显示输入质量报告

HMI 不得只显示：

```text
导入失败
```

必须显示：

```text
DXF 输入质量报告
错误数量
警告数量
阻断阶段
阻断原因
问题图层
问题 entity
问题坐标
建议修复动作
是否允许继续
```

## 15.2 操作员消息要求

`operator_message` 必须可被非研发人员理解。

示例：

```text
禁止执行：
GLUE_CONTOUR 图层存在 3 个断点。
最大断点 0.86 mm，超过允许值 0.10 mm。
位置：[132.45, 58.20]。
请返回 CAD 修复轮廓闭合性后重新导入。
```

禁止只显示：

```text
parse failed
invalid geometry
unknown exception
error code 1003
```

除非同时显示明确解释。

---

# 16. 与预览系统的关系

## 16.1 预览不是 DXF 原图显示

HMI 预览不得只是把 DXF 画出来。
预览必须逐步升级为：

```text
执行前仿真 / 验证系统
```

## 16.2 输入治理问题必须可视化

预览必须能显示：

```text
断点
重复线
极短段
自交位置
越界位置
未知图层
不支持 entity 位置
拟合误差风险
禁止区域冲突
```

## 16.3 预览必须追溯到执行前模型

最终预览不应直接来自原始 DXF。
推荐链路：

```text
SourceDrawing
  -> CanonicalGeometry
  -> TopologyModel
  -> FeatureGraph
  -> ProcessPlan
  -> ProcessPath
  -> PathQualityReport
  -> ExecutionPreviewModel
```

预览中每个风险标记必须能追溯：

```text
source_drawing_ref
source_hash
source_entity_id
source_layer
validation_error_code
stage_id
owner_module
```

---

# 17. 与路径质量门禁的关系

DXF 输入治理只判断输入是否合格。
路径质量门禁判断生成后的路径是否适合执行。
二者都必须通过。

标准关系：

```text
S1 SourceDrawing accepted
  ↓
S2 CanonicalGeometry built
  ↓
S3 TopologyModel built
  ↓
S4 FeatureGraph built
  ↓
S5 ProcessPlan built
  ↓
S8 ProcessPath built
  ↓
PathQualityAnalyzer
  ↓
PathQualityGate PASS
  ↓
S9 MotionPlan
  ↓
S10 DispenseTimingPlan
  ↓
S11A ExecutionPackageBuilt
  ↓
S11B ExecutionPackageValidated
```

禁止：

```text
DXF 输入治理 FAIL 后仍生成可执行路径；
PathQualityGate FAIL 后仍允许 HMI 执行；
Runtime 自行绕过 DXF 和路径质量门禁。
```

---

# 18. 与执行安全门禁的关系

S12 `MachineReadySnapshot` / `Preflight` 必须检查 DXF 输入治理和路径质量结果。

至少包含：

```text
source_drawing_valid == true
canonical_geometry_valid == true
topology_model_valid == true
feature_graph_valid == true
process_path_valid == true
path_quality_gate_result == PASS
execution_package.validation_result == passed
workspace_bounds_pass == true
interpolation_segments_ready == true
```

如果输入无效，必须返回：

```text
failure_stage = "dxf_input_validation" 或对应 S1/S2/S3/S4 阶段
error_code = 对应 DXFValidationErrorCode
operator_message = DXFValidationReport 中的 operator_message
diagnostic_record = 对应 report 引用
```

重要边界：

```text
PreflightBlocked 是执行门禁阻断，
不是 S1-S11 规划对象失败。
设备未 ready 不得导致 SourceDrawing / CanonicalGeometry / ProcessPlan / MotionPlan 被改写或失效。
```

---

# 19. 追溯要求

## 19.1 每次导入必须留痕

必须记录：

```text
job_id
import_id
source_drawing_ref
file_name
file_hash
DXFInputSpec version
DXFValidationPolicy version
GeometryTolerancePolicy version
LayerSemanticPolicy version
software version
algorithm version
machine_config version
recipe snapshot id
DXFValidationReport path/ref
GeometryQualityReport path/ref
TopologyQualityReport path/ref
gate_result
```

## 19.2 失败任务必须可复现

对于任一失败任务，必须能够回答：

```text
导入的是哪个 DXF？
文件 hash 是什么？
使用了哪版输入规范？
使用了哪版 policy？
失败在哪个阶段？
哪个 error_code 阻断？
问题坐标在哪里？
HMI 给操作员显示了什么？
是否生成过修复建议？
是否进入过路径生成？
是否进入过执行安全门禁？
```

## 19.3 S16 归档要求

`ExecutionRecord` 至少应能回链：

```text
SourceDrawing
CanonicalGeometry
TopologyModel
FeatureGraph
ProcessPlan
ProcessPath
PathQualityReport
ExecutionPackage
MachineReadySnapshot
FirstArticleResult
fault history
final workflow state
```

---

# 20. 自动修复策略

## 20.1 默认不自动修复

第一阶段默认：

```text
Detect Only
Report Only
Reject If Blocking
```

## 20.2 允许的受控修复

仅允许以下低风险修复进入候选：

```text
统一轮廓方向
去除非执行图层空对象
标准化图层名大小写
将轻微重复 vertex 合并为一个点
```

条件：

```text
必须记录
必须可回滚
必须重新验证
必须在 report 中说明
必须有 fixture 测试
```

## 20.3 高风险修复

以下修复属于高风险，不得静默执行：

```text
连接断点
删除重复线
删除重叠线
展开 block
采样 spline
转换 ellipse
推断单位
推断图层语义
自动补对位点
自动修改点胶轮廓
```

高风险修复必须遵循：

```text
Detect
Report
Propose Fix
Preview Diff
User Confirm
Revalidate
Accept or Reject
Trace
```

## 20.4 修复后的对象版本规则

任何导致 `CanonicalGeometry`、`TopologyModel` 或 `FeatureGraph` 变化的修复，都必须遵守：

```text
不得覆盖 SourceDrawing；
不得原地修改旧对象；
必须创建新版本；
必须显式 supersede 下游旧对象；
必须重新运行对应阶段验证；
必须记录 repair action 与 revalidation report。
```

---

# 21. Fixture 体系

## 21.1 Golden Fixtures

目录：

```text
tests/fixtures/dxf/golden/
```

建议包含：

```text
simple_line.dxf
simple_arc.dxf
simple_circle.dxf
simple_closed_contour.dxf
simple_polyline_contour.dxf
multi_contour_valid.dxf
bra_outline_valid.dxf
rect_diag.dxf
```

Golden fixture 必须稳定通过：

```text
S1 SourceDrawing
S2 CanonicalGeometry
S3 TopologyModel
S4 FeatureGraph
```

## 21.2 Pathological Fixtures

目录：

```text
tests/fixtures/dxf/pathological/
```

必须包含：

```text
empty_file.dxf
wrong_units.dxf
missing_units.dxf
unsupported_spline.dxf
unsupported_ellipse.dxf
block_insert.dxf
hatch_entity.dxf
3d_entity.dxf
unknown_layer.dxf
empty_glue_layer.dxf
open_contour.dxf
broken_contour.dxf
tiny_segments.dxf
duplicated_lines.dxf
overlapping_entities.dxf
self_intersection.dxf
workspace_out_of_bounds.dxf
```

Pathological fixture 必须稳定产生：

```text
明确 error_code
明确 severity
明确 gate_result
明确 operator_message
明确阻断阶段
```

## 21.3 Expected Reports

每个 pathological fixture 应有 expected report：

```text
tests/fixtures/dxf/expected_reports/
  unsupported_spline.expected.json
  block_insert.expected.json
  open_contour.expected.json
  duplicated_lines.expected.json
  wrong_units.expected.json
```

测试必须断言：

```text
error_code 稳定
severity 稳定
gate_result 稳定
blocking 行为稳定
stage_id 稳定
schema 稳定
```

---

# 22. CI 门禁

## 22.1 Quick Gate

每次提交或 Codex 修改后应运行：

```text
DXFInputValidator unit tests
DXFValidationReport schema tests
DXFValidationErrorCode stability tests
Golden fixture smoke tests
Pathological fixture smoke tests
API shape tests
HMI offline smoke relevant to import failure display
```

## 22.2 Full Offline Gate

PR 或每日运行：

```text
Golden DXF full validation
Pathological DXF full rejection
S1 -> S4 object-chain consistency
DXF -> CanonicalGeometry consistency
DXF -> TopologyModel consistency
DXF -> FeatureGraph consistency
DXF -> ProcessPath blocked/pass behavior
Preview risk overlay consistency
Diagnostic bundle generation
```

## 22.3 Nightly Performance Gate

夜间运行：

```text
大 DXF 解析性能
大量 entity 验证性能
拓扑分析性能
report 生成性能
内存占用
异常文件处理耗时
```

---

# 23. Error Code 命名规范

## 23.1 命名格式

```text
<DOMAIN>_<SPECIFIC_REASON>
```

推荐 DOMAIN：

```text
DXF
GEOM
TOPOLOGY
FEATURE
PROCESS
GATE
TRACE
```

示例：

```text
DXF_UNIT_MISSING
DXF_BLOCK_INSERT_NOT_EXPLODED
GEOM_TINY_SEGMENT
TOPOLOGY_OPEN_CONTOUR
FEATURE_UNCLASSIFIED_GLUE_GEOMETRY
PROCESS_NO_GLUE_GEOMETRY
GATE_DXF_INPUT_FAILED
TRACE_DXF_HASH_MISSING
```

## 23.2 Error Code 稳定性

已发布 error_code 不得随意改名。

如必须废弃：

```text
保留兼容映射
记录 migration
更新 HMI 文案
更新 expected reports
更新诊断解析逻辑
```

---

# 24. 配置与 Policy

## 24.1 DXFValidationPolicy

`DXFValidationPolicy` 必须集中表达：

```text
允许 DXF 版本
允许 entity 类型
禁止 entity 类型
图层语义映射
tolerance 参数
warning/error 分级
是否允许某些非执行对象
最大 entity 数
最大文件大小
最大 bbox
是否允许自动修复
```

## 24.2 Policy Snapshot

每次任务必须记录 policy snapshot。

原因：

```text
同一个 DXF 在不同 policy 下可能有不同 gate_result；
追溯时必须知道当时用的是哪套规则。
```

---

# 25. 非目标

第一阶段明确不做：

```text
无限兼容所有 DXF
自动修复所有 CAD 错误
AI 自动理解任意图层语义
直接从复杂 CAD 图纸推断点胶工艺
将 SPLINE 默认采样后执行
将 HATCH 区域默认转换为点胶区域
忽略错误继续生成可执行路径
```

---

# 26. 代码落地建议

建议新增或冻结以下服务、policy、report 与 schema：

```text
DXFInputSpec
DXFValidationPolicy
DXFInputValidator
DXFValidationReport
DXFValidationError
DXFValidationSeverity
DXFValidationErrorCode
DXFEntitySummary
DXFLayerSummary
DXFGeometrySummary
DXFTopologySummary
GeometryTolerancePolicy
LayerSemanticPolicy
```

注意：以上不是替代 `SourceDrawing`、`CanonicalGeometry`、`TopologyModel`、`FeatureGraph` 的正式对象，而是支撑这些正式对象的服务和诊断结构。

---

# 27. 最小实现顺序

不得从大规模重构开始。

推荐顺序：

```text
1. 在 docs/architecture/dsp-e2e-spec/README.md 注册本文档为冻结补充标准
2. 冻结 DXFInputSpec
3. 建立 DXFValidationErrorCode
4. 建立 DXFValidationReport schema
5. 建立 DXFValidationPolicy
6. 建立 DXFInputValidator 骨架
7. 接入 golden/pathological fixtures
8. 实现 S1 文件级检查
9. 实现 S2 entity / 单位 / 坐标检查
10. 实现 S3 几何质量 / 拓扑质量检查
11. 实现 S4 图层语义 / FeatureGraph 前置检查
12. 接入阶段阻断事件
13. HMI 显示报告
14. 接入 trace-diagnostics
15. 接入 CI quick gate
16. 接入 full offline gate
```

---

# 28. Definition of Done

DXF 输入治理 v1 只有在以下条件全部满足时，才视为完成：

```text
本文档已注册到 dsp-e2e-spec/README.md 的冻结补充标准表
DXFInputSpec 已冻结
DXFValidationErrorCode 已稳定
DXFValidationReport schema 已测试
DXFValidationPolicy 可配置并可追溯
DXFInputValidator 能独立运行
合法 golden fixture 稳定通过
非法 pathological fixture 稳定拒绝
错误报告包含 reason / layer / entity_id / 坐标或近似位置
S1/S2/S3/S4 阶段阻断事件正确产生
DXF 输入治理 FAIL 时不能进入正式路径生成
HMI 能显示导入失败原因
任务追溯中记录 SourceDrawing hash 与 validation report
CI quick gate 覆盖 DXF validator
full offline gate 覆盖 golden/pathological fixture
```

---

# 29. Frozen Conclusion

DXF 输入治理的最终标准不是“能读取 DXF”，而是：

```text
上位机必须把 DXF 作为受控工程输入处理。

任何 DXF 在进入路径生成前，必须先通过 S1 文件封存、S2 几何标准化、S3 拓扑质量、S4 制造特征与图层语义检查。

任何不满足规范的 DXF，都必须被拒绝或进入明确人工修复流程。

任何拒绝都必须有 error_code、operator_message、问题位置、诊断记录和回归样例。

任何通过的 DXF，都必须能追溯到 SourceDrawing hash、entity、图层、policy、算法版本和后续执行语义。
```

换句话说：

```text
DXF 输入治理是点胶执行质量的第一道工业级防线。
它的目标不是让系统“尽量跑起来”，而是确保系统“不可能错跑”。
```
