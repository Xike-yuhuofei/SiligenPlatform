# fragmented_chain

`fragmented_chain.pb` 是 `process-path` 的 repo-owned repair regression fixture。

用途：

- 冻结 `TopologyRepairPolicy::Auto` 对断裂线链的主路径修补语义
- 验证同一组 primitives 在 `Off` 与 `Auto` 下会产生不同的 fragmentation / rapid / dispense diagnostics

当前样本构成：

- 4 条线段
- 几何上可闭合为矩形，但输入顺序故意打散为断裂链

期望语义：

- `Off`：保留断裂顺序，出现多段 dispense fragment
- `Auto`：允许主链修补并降低 `discontinuity_after`

该样本不是生产工艺基线，不参与 preview / simulation canonical producer 输出，只服务于 `M6 process-path` 的 repair regression。
