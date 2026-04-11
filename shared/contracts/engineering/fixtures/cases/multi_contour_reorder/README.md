# multi_contour_reorder

`multi_contour_reorder.pb` 是 `process-path` 的 repo-owned contour ordering regression fixture。

用途：

- 冻结 topology repair 在多 contour 输入下的 reorder / reverse / rotate diagnostics
- 防止 contour 排序语义再次退回为隐式、不可观测行为

当前样本构成：

- 1 组开放线链
- 1 条反向开放线段
- 1 个起点被刻意旋转的闭合方形 contour

期望语义：

- `Off`：保留输入次序，允许立即回折或非最优 contour 起点
- `Auto`：触发至少一种 contour ordering 修正，并更新 `reordered_contours` / `reversed_contours` / `rotated_contours`

该样本不是生产工艺基线，不参与 preview / simulation canonical producer 输出，只服务于 `M6 process-path` 的 contour ordering regression。
