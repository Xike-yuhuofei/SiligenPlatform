# samples/golden

`samples/golden/` 是 `M3 topology-feature` 相关 golden 输入与基准样本的正式承载面（canonical root）。

## 归位关系

- `samples/golden/README.md` 是 golden 资产的正式入口，并受 `samples/README.md` 统一索引。
- `examples/`（含历史 golden 路径）仅保留迁移来源与 redirect/tombstone 角色。

## 正式职责

- 承载用于拓扑/特征提取校验的 golden 输入与基准对照。
- 为上游链回归、集成测试和验收门禁提供稳定基线。
- 作为特征对象边界冻结后的样本事实源。

## 输入范围

- golden 输入样本（原始输入与对应期望基线）
- 基准说明文档（输入来源、版本和适用验证范围）
- 回归引用清单

## 维护与回流约束

- 历史路径（含 `examples/`）只允许作为迁移来源，不再作为终态承载面。
- 新增长期 golden case 必须写入 `samples/golden/` 并附带用途说明。
- 禁止将稳定 golden 输入回流到 legacy roots（尤其 `examples/`）。
