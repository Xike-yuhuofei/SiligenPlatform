# arc_circle_quadrants

`arc_circle_quadrants` 是 `shared/contracts/engineering` 下的 full-chain canonical producer case。

用途：

- 冻结 arc-only 闭合轮廓在 `DXF -> PB -> preview -> simulation-input` 下的稳定输出
- 为 closed-loop arc 语义提供独立于 polyline 的跨 owner 共同真值

当前样本构成：

- 4 段四分之一圆弧
- 闭合圆形轮廓
- 不包含 fallback-only 或诊断性噪声图元

期望语义：

- `dxf_to_pb` 导出为 4 个原生 `ARC` primitive
- `generate_preview.py` 输出稳定 preview 指标
- `bundle_to_simulation_payload()` 保留 4 个 `ARC` path segments

该样本参与 preview / simulation / HMI / e2e 的 canonical producer 契约，但不是默认 performance 或 HIL 输入。
