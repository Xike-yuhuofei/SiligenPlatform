# topology-feature

`M3 topology-feature`

## 模块职责

- 承接 `S3-S4`，完成拓扑修复、连通性建模与制造特征提取。

## Owner 对象

- `TopologyModel`
- `FeatureGraph`

## 输入契约

- `BuildTopology`
- `ExtractFeatures`
- `SupersedeOwnedArtifact`

## 输出契约

- `TopologyBuilt`
- `TopologyRejected`
- `FeatureGraphBuilt`
- `FeatureClassificationFailed`
- `ArtifactSuperseded`

## 禁止越权边界

- 不得写工艺速度、阀延时。
- 不得生成 `ProcessPlan`。

## 测试入口

- `S09` topology rebuild/feature extraction failure
