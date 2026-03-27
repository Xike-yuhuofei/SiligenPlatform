# dxf-geometry

`M2 dxf-geometry`

## 模块职责

- 承接 `S2`，把 `SourceDrawing` 解析成 `CanonicalGeometry`。
- 统一 DXF 实体、单位、兼容性标记和几何标准化语义。

## Owner 对象

- `CanonicalGeometry`

## 输入契约

- `ParseSourceDrawing`
- `RebuildCanonicalGeometry`
- `SupersedeOwnedArtifact`

## 输出契约

- `CanonicalGeometryBuilt`
- `CanonicalGeometryRejected`
- `ArtifactSuperseded`

## 禁止越权边界

- 不得做特征分类。
- 不得改写 `SourceDrawing`。
- 不得引入设备动态参数。

## 测试入口

- `S09` DXF parse success/unsupported entity/unit normalization
