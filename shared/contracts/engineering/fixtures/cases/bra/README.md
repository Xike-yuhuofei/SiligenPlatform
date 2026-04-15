# bra

`bra` 是 `shared/contracts/engineering` 下的 full-chain canonical producer case。

用途：

- 冻结闭合有机 `LWPOLYLINE` 轮廓在 `DXF -> PB -> preview -> simulation-input` 下的稳定输出
- 为 closed-loop polyline 语义提供除 `rect_diag` 之外的第二个跨 owner 共同真值

当前样本构成：

- 1 个闭合 `LWPOLYLINE` 轮廓
- 不包含 `POINT`、`TEXT`、`INSERT` 等 runtime 不可执行辅助图元

期望语义：

- `dxf_to_pb` 导出为 1 个闭合 `CONTOUR`
- `generate_preview.py` 输出稳定 preview 指标
- `bundle_to_simulation_payload()` 导出纯 `LINE` path segments

该样本参与 preview / simulation / HMI / e2e 的 canonical producer 契约，但不是默认 performance 或 HIL 输入。
