# DXF 输入契约 v1

## 1. 目标

本契约用于约束点胶机上位机对 DXF 文件的读取、校验、归一化与导入结果判定，确保：

- 输入格式可控
- 解析行为稳定
- 业务层不受 DXF 版本差异污染
- 规划链路只消费统一后的标准几何模型
- 导入失败具有明确、可诊断、可追踪的错误口径

---

## 2. 适用范围

本契约适用于以下链路：

```text
DXF 文件
→ 导入器
→ 几何归一化
→ 工艺切分
→ 轨迹规划
→ 插补程序构建
→ 执行下发
```

本契约不负责：

- 通用 CAD 全语义还原
- 打印/布局/视口语义
- 标注语义保真
- 渲染/材质/图层视觉表现
- 3D 模型处理

---

## 3. 标准协同基线

### 3.1 对外交付标准

```text
标准输入格式：ASCII DXF
标准协同版本：AutoCAD R2000（AC1015）
标准单位：mm
标准空间：Modelspace
标准维度：2D
```

### 3.2 兼容读取范围

系统可兼容读取：

- R12（AC1009）
- R2000（AC1015）
- R2004（AC1018）
- R2007（AC1021）
- R2010（AC1024）
- R2013（AC1027）
- R2018（AC1032）

### 3.3 原则

- R2000 是标准生产协同格式
- R12 是降级兼容格式
- 高版本仅保证白名单二维实体可读，不保证保留原始高版本扩展语义

---

## 4. 文件级输入规则

### 4.1 接受

- 扩展名为 `.dxf`
- ASCII DXF
- Binary DXF（可选支持，不作为默认交付格式）

### 4.2 拒绝

- `.dwg`
- 伪装成 DXF 的非 DXF 文件
- 头部损坏或结构损坏的 DXF
- 无法识别编码且无法恢复的文件
- 无有效几何对象的空文件
- 仅包含布局/标注/3D 对象而无生产几何的文件

---

## 5. 空间与维度规则

### 5.1 空间规则

仅处理：

- `Modelspace`

不处理：

- `Paperspace`
- Layout
- Viewport
- 打印相关对象

### 5.2 维度规则

仅接受可归一化为 2D 生产几何的对象。

满足以下任一情况则拒绝进入生产链路：

- 3D 实体
- 不可唯一投影为 2D 路径
- Z 轴几何语义影响实际工艺但未被明确定义
- 非平面对象

---

## 6. 单位规则

### 6.1 内部唯一长度单位

系统内部唯一长度单位为：

```text
mm
```

### 6.2 导入时单位解析顺序

导入器应按以下顺序确定单位：

1. 读取 `$INSUNITS`
2. 若存在且合法，则转换到 mm
3. 若不存在，按配置决定：
   - 预览模式：可按默认 mm 解释并告警
   - 生产模式：禁止下发

### 6.3 单位处理策略

| 场景 | 处理 |
|---|---|
| `$INSUNITS=mm` | 直接使用 |
| `$INSUNITS=inch` | 转 mm |
| `$INSUNITS=cm` | 转 mm |
| `$INSUNITS=m` | 转 mm |
| `$INSUNITS` 缺失 | 预览可告警，生产禁止 |

---

## 7. 实体支持矩阵

### 7.1 P0：必须支持

| 实体 | 是否导入 | 是否参与路径 | 备注 |
|---|---:|---:|---|
| LINE | 是 | 是 | 基础线段 |
| ARC | 是 | 是 | 基础圆弧 |
| CIRCLE | 是 | 是 | 可视为闭环 |
| LWPOLYLINE | 是 | 是 | 优先支持 |
| POLYLINE（2D） | 是 | 是 | 需验证 2D 性 |
| POINT | 是 | 视配置 | 可作为工艺点 |

### 7.2 P1：建议支持

| 实体 | 是否导入 | 是否参与路径 | 备注 |
|---|---:|---:|---|
| ELLIPSE | 是 | 是 | 规划前离散或保留高阶 |
| SPLINE | 是 | 是 | 规划前离散或保留高阶 |
| INSERT | 是 | 是 | 必须先展开 |
| TEXT | 是 | 否 | 仅告警/忽略 |
| MTEXT | 是 | 否 | 仅告警/忽略 |

### 7.3 P2：忽略或拒绝

| 实体 | 默认策略 | 备注 |
|---|---|---|
| DIMENSION | 忽略并告警 | 不参与生产 |
| LEADER / MLEADER | 忽略并告警 | 不参与生产 |
| HATCH | 默认拒绝 | 除非单独定义填充转路径规则 |
| XLINE / RAY | 拒绝 | 非有限生产几何 |
| IMAGE / PDFUNDERLAY | 拒绝 | 非几何生产对象 |
| 3DFACE | 拒绝 | 3D |
| REGION | 拒绝 | 语义过宽 |
| SOLID / BODY / SURFACE / MESH | 拒绝 | 3D 或不可控 |
| 代理对象 | 拒绝 | 不可控依赖 |

---

## 8. 块参照 `INSERT` 规则

### 8.1 总原则

`INSERT` 不得直接进入业务层。  
必须在导入层展开为显式几何。

### 8.2 展开流程

```text
INSERT
→ 读取块定义
→ 应用平移/旋转/缩放
→ 递归展开子块
→ 输出最终几何集合
→ 进入归一化阶段
```

### 8.3 拒绝条件

以下情况直接报错：

- 循环块引用
- 递归深度超限
- 块中包含拒绝实体
- 变换后几何不可恢复
- 非等比缩放导致 ARC/CIRCLE 语义失真且系统未定义转换规则

---

## 9. 高阶曲线规则

### 9.1 ELLIPSE

处理策略二选一：

- 保留为内部高阶几何，规划前离散
- 导入时直接离散为 polyline

推荐：

- 导入层保留
- 规划前离散

### 9.2 SPLINE

不建议在执行层长期保留原生样条语义。  
建议在规划前统一按设备精度离散。

### 9.3 离散参数建议

```yaml
curve_flatten:
  chord_error_mm: 0.02
  max_segment_length_mm: 1.0
  min_segment_length_mm: 0.05
  angle_step_deg: 2.0
  preserve_closed_flag: true
```

具体参数应由设备精度、工艺宽度、轨迹平滑性要求共同决定。

---

## 10. 归一化规则

### 10.1 总原则

DXF 导入后，必须进入内部标准几何模型。  
业务层、工艺层、轨迹层、执行层不得再依赖 DXF 版本分支。

### 10.2 内部标准几何建议

```text
LinePrimitive
ArcPrimitive
CirclePrimitive
PolylinePrimitive
EllipsePrimitive
SplinePrimitive
PointPrimitive
```

或在更强收敛策略下：

```text
PointSet
Polyline
ArcSegment
ClosedLoop
```

### 10.3 归一化内容

- 单位归一化：全部转 mm
- 空间归一化：全部落到统一二维平面
- 拓扑归一化：开路径 / 闭路径 / 孤立点
- 方向归一化：保留原向或统一口径
- 几何合法性校验：零长度、重复点、自交、断裂等

---

## 11. 导入状态机

建议冻结如下状态机。

```text
Idle
→ FileOpened
→ HeaderParsed
→ VersionAccepted
→ UnitResolved
→ EntityScanned
→ EntityExpanded
→ GeometryNormalized
→ GeometryValidated
→ ImportReady
```

失败出口：

```text
HeaderParseFailed
VersionRejected
UnitRejected
EntityRejected
BlockExpandFailed
GeometryInvalid
ImportFailed
```

---

## 12. 导入结果分级

### 12.1 Success

条件：

- 关键几何全部有效
- 单位明确
- 无阻断错误
- 可进入工艺规划和执行生成

### 12.2 SuccessWithWarnings

条件：

- 存在被忽略的非生产对象
- 存在可恢复问题但已成功修复
- 可进入生产链路

### 12.3 PreviewOnly

条件：

- 可预览
- 但存在阻断生产的问题

例如：

- 单位缺失
- 非法闭环待人工确认
- 块展开异常但部分图形可见
- 存在不可接受的几何修复

### 12.4 Failed

条件：

- 文件不可解析
- 版本不支持
- 无有效生产几何
- 关键实体无法恢复
- 必须拒绝下发

---

## 13. 错误码建议

### 13.1 文件级

| 错误码 | 含义 |
|---|---|
| `DXF_E_FILE_NOT_FOUND` | 文件不存在 |
| `DXF_E_FILE_OPEN_FAILED` | 文件打开失败 |
| `DXF_E_INVALID_SIGNATURE` | 非法 DXF 文件头 |
| `DXF_E_CORRUPTED_FILE` | 文件结构损坏 |

### 13.2 版本/编码

| 错误码 | 含义 |
|---|---|
| `DXF_E_UNSUPPORTED_VERSION` | 不支持的 DXF 版本 |
| `DXF_E_ENCODING_UNKNOWN` | 编码无法识别 |
| `DXF_E_BINARY_NOT_ALLOWED` | 当前配置禁止 Binary DXF |

### 13.3 单位

| 错误码 | 含义 |
|---|---|
| `DXF_E_UNIT_MISSING` | 单位缺失 |
| `DXF_E_UNIT_UNSUPPORTED` | 单位不支持 |
| `DXF_E_UNIT_REQUIRED_FOR_PRODUCTION` | 生产模式要求明确单位 |

### 13.4 实体扫描

| 错误码 | 含义 |
|---|---|
| `DXF_E_NO_VALID_ENTITY` | 无有效几何实体 |
| `DXF_E_UNSUPPORTED_ENTITY` | 存在不支持实体 |
| `DXF_E_3D_ENTITY_FOUND` | 检测到 3D 实体 |
| `DXF_E_PROXY_ENTITY_FOUND` | 检测到代理对象 |

### 13.5 块展开

| 错误码 | 含义 |
|---|---|
| `DXF_E_BLOCK_NOT_FOUND` | 块定义不存在 |
| `DXF_E_BLOCK_RECURSION` | 块递归引用 |
| `DXF_E_BLOCK_DEPTH_EXCEEDED` | 块嵌套过深 |
| `DXF_E_BLOCK_EXPAND_FAILED` | 块展开失败 |

### 13.6 几何合法性

| 错误码 | 含义 |
|---|---|
| `DXF_E_ZERO_LENGTH_SEGMENT` | 零长度线段 |
| `DXF_E_INVALID_ARC_PARAM` | 圆弧参数非法 |
| `DXF_E_INVALID_SPLINE` | 样条非法 |
| `DXF_E_SELF_INTERSECTION` | 路径自交 |
| `DXF_E_OPEN_LOOP_UNEXPECTED` | 需要闭环但检测为开环 |
| `DXF_E_DUPLICATE_VERTEX` | 重复顶点异常 |
| `DXF_E_GEOMETRY_OUT_OF_RANGE` | 超出工作区 |

---

## 14. 告警码建议

不是所有问题都该报错，很多只需要告警。

| 告警码 | 含义 |
|---|---|
| `DXF_W_TEXT_IGNORED` | 文字已忽略 |
| `DXF_W_DIMENSION_IGNORED` | 标注已忽略 |
| `DXF_W_UNIT_ASSUMED_MM` | 单位缺失，按 mm 假定 |
| `DXF_W_HIGH_VERSION_DOWNGRADED` | 高版本语义已降级 |
| `DXF_W_ENTITY_FLATTENED` | 高阶曲线已离散 |
| `DXF_W_SMALL_GAP_FIXED` | 小间隙已自动修复 |
| `DXF_W_DUPLICATE_POINT_REMOVED` | 重复点已去除 |

---

## 15. 导入配置建议

建议把配置显式化，不要把口径写死在代码里。

```yaml
dxf_import:
  collaboration_baseline: R2000
  readable_versions:
    - R12
    - R2000
    - R2004
    - R2007
    - R2010
    - R2013
    - R2018

  allow_ascii: true
  allow_binary: true
  require_modelspace: true

  unit:
    internal_unit: mm
    require_for_production: true
    default_if_missing_for_preview: mm

  entity_policy:
    path_entities:
      - LINE
      - ARC
      - CIRCLE
      - LWPOLYLINE
      - POLYLINE
      - POINT
      - ELLIPSE
      - SPLINE
      - INSERT
    ignored_entities:
      - TEXT
      - MTEXT
      - DIMENSION
      - LEADER
      - MLEADER
    rejected_entities:
      - XLINE
      - RAY
      - IMAGE
      - PDFUNDERLAY
      - 3DFACE
      - REGION
      - BODY
      - SURFACE
      - MESH
      - PROXY

  block_policy:
    expand_insert: true
    max_expand_depth: 8
    reject_recursive_insert: true

  flatten_policy:
    enable_for_spline: true
    enable_for_ellipse: true
    chord_error_mm: 0.02
    max_segment_length_mm: 1.0
    min_segment_length_mm: 0.05

  topology_policy:
    remove_zero_length_segment: true
    remove_duplicate_vertex: true
    auto_fix_small_gap: true
    small_gap_tolerance_mm: 0.02

  workspace_policy:
    validate_bounds: true
    x_min_mm: 0
    x_max_mm: 1600
    y_min_mm: 0
    y_max_mm: 800
```

---

## 16. 生产模式与预览模式必须分开

这是点胶机上位机非常关键的一条。

### 16.1 预览模式

目标：

- 尽可能让用户看到图
- 尽可能提供诊断信息
- 允许可控降级

可接受：

- 单位缺失但默认 mm
- 忽略文本/标注
- 高阶曲线离散
- 部分非关键警告

### 16.2 生产模式

目标：

- 保证执行安全与可预测性
- 不允许关键歧义

必须拒绝：

- 单位不明
- 关键几何不可恢复
- 非法块展开
- 路径拓扑不合法
- 超工作区
- 需要闭环而未闭环

---

## 17. 验收样例建议

建议建立一组固定 DXF 样例，作为导入契约测试集。

### 17.1 应成功样例

- 单 LINE 文件
- ARC + LINE 组合
- CIRCLE 闭环
- 含 bulge 的 LWPOLYLINE
- 可展开的 INSERT
- 合法 ELLIPSE
- 合法 SPLINE

### 17.2 应告警但成功样例

- 含 TEXT / DIMENSION
- 单位缺失但在预览模式
- 高版本文件但仅包含白名单实体
- 少量重复点可自动修复

### 17.3 应仅预览样例

- 单位缺失且当前为生产模式
- 存在部分不可恢复对象但主图可显示
- 块中存在忽略对象但存在潜在歧义

### 17.4 应失败样例

- 文件损坏
- 不支持版本
- 仅 3D 对象
- 递归块引用
- 非法样条
- 自交闭环且无法修复
- 几何全部超出工作区

---

## 18. 对代码架构的约束

建议把 DXF 导入层边界写死，避免后续污染。

### 18.1 必须成立

- DXF 版本差异只存在于导入适配层
- 规划层不读取 `$ACADVER`
- 执行层不关心原始实体类型
- HMI 预览与生产规划共享同一份归一化几何
- 错误码稳定、可测试、可回归

### 18.2 不允许出现

- 业务层根据 DXF 版本分支
- 规划层直接消费原始 DXF entity
- 导入器悄悄忽略关键错误
- 预览链路与生产链路几何口径不一致
- 不可追踪的自动修复

---

## 19. 最终推荐冻结口径

你可以直接把这一段写到规范首页：

```text
点胶机上位机的标准 DXF 输入格式为 ASCII DXF，标准协同版本为 AutoCAD R2000（AC1015），标准单位为 mm，标准空间为 Modelspace，标准维度为二维生产几何。

系统可兼容读取 R12 至 R2018 的 DXF 文件，但所有版本差异必须在导入层完成吸收与归一化。业务层、工艺层、轨迹层与执行层不得依赖 DXF 版本分支。

系统仅保证白名单二维几何实体的读取与生产语义转换，不保证保留通用 CAD 的全部对象语义。单位不明、关键几何不可恢复、块展开失败、存在 3D/代理对象或路径拓扑不合法的文件，不得生成生产执行数据。
```

---

## 20. 简明结论

### 建议默认版本

**R2000 ASCII DXF**

### 建议兼容下限

**R12 ASCII DXF**

### 建议程序策略

**多版本可读，单一内部模型，版本差异只在导入层消化**
