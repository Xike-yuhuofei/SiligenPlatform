# Contract: DXF Open Auto-Preview Timeout Boundary

## 1. 目的

冻结“打开 DXF 自动触发的胶点预览”在本特性中的本地等待边界，确保 HMI 只在这条用户链路上将超时阈值调整为 `100s`，而不会把变更悄悄扩散到其他入口。

## 2. 适用边界

- UI 入口: `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\ui\main_window.py`
- worker owner: `D:\Projects\SiligenSuite\modules\hmi-application\application\hmi_application\preview_session.py`
- client protocol: `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\client\protocol.py`
- transport timeout source: `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\client\tcp_client.py`
- formal wire schema reference: `D:\Projects\SiligenSuite\shared\contracts\application\commands\dxf.command-set.json`

## 3. 覆盖的请求阶段

本 contract 只覆盖以下两段串行请求：

1. `dxf.plan.prepare`
2. `dxf.preview.snapshot`

两者都属于 `PreviewSnapshotWorker` 在 DXF 工件上传成功后自动发起的预览生成链路。

## 4. 超时规则

| 请求 | 本特性要求 |
|---|---|
| `dxf.plan.prepare` | 在 DXF 打开自动预览链路上必须使用 `100s` 本地等待预算 |
| `dxf.preview.snapshot` | 在 DXF 打开自动预览链路上必须使用 `100s` 本地等待预算 |
| `dxf.preview.confirm` | 不在本特性范围内，保持既有预算 |
| runtime resync `dxf.preview.snapshot` | 不在本特性范围内，保持既有预算 |
| `dxf.job.start` / `connect` / `dxf.load` | 不在本特性范围内，保持既有预算 |

## 5. 成功条件

以下条件全部满足时，本 contract 视为成功：

1. 自动预览链路中的 prepare 与 snapshot 都不再在 `15.0s` 时过早失败。
2. 任一阶段在 `100s` 内完成时，链路能够继续推进或成功展示预览。
3. 任一阶段超过 `100s` 时，链路在达到预算后才返回 timeout。
4. 不在本 contract 范围内的其他请求，未被动继承 `100s`。

## 6. 失败边界

出现以下任一情况时，本 contract 视为失败：

1. prepare 仍使用 `15.0s` 或其他旧阈值。
2. snapshot 仍使用 `15.0s` 或其他旧阈值。
3. runtime resync、`dxf.preview.confirm`、`dxf.job.start` 等无关入口被一并放大到 `100s`。
4. 为了实现 `100s` 而修改了 `dxf.command-set.json` 的 wire schema。

## 7. 与正式协议的关系

本文件不新增任何网络协议字段，也不替代 `dxf.command-set.json`。它只冻结 HMI 本地等待策略与适用边界，用于后续 tasks、实现与回归设计。
