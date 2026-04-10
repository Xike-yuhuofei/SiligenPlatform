# point_mixed

`point_mixed.pb` 是 `process-path` 的 repo-owned rejection fixture。

用途：

- 冻结 `PbPathSourceAdapter -> ProcessPathFacade` 链路在遇到 `Point primitive` 时的稳定拒绝语义
- 防止 Point 在不同 repair policy 下被隐式吞掉或变形成零长度段

当前样本构成：

- `Line`
- `Point`
- `Line`

该样本不是生产工艺基线，不参与 preview / simulation canonical producer 输出，只服务于 `M6 process-path` 的 rejection regression。
