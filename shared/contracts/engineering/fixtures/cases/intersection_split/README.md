# intersection_split

`intersection_split.pb` 是 `process-path` 的 repo-owned split-intersection regression fixture。

用途：

- 冻结 topology repair 对交叉线段的 intersection diagnostics 与 split 结果
- 防止 repair 策略退回到只统计交点、不产生稳定重建输出

当前样本构成：

- 2 条在中心交叉的对角线
- 1 条从交点后续延伸的尾段

期望语义：

- `Off`：保留原始 primitive 数量，交点不被拆分
- `Auto`：允许识别交点并增加 `repaired_primitive_count`

该样本不是生产工艺基线，不参与 preview / simulation canonical producer 输出，只服务于 `M6 process-path` 的 split-intersection regression。
