# Contract: Preview Timeout Error Propagation

## 1. 目的

定义 timeout 错误从 transport 到用户可见 UI 的传播约束，确保当自动预览真正超过 `100s` 时，用户看到的是反映新阈值的超时信息，而不是过期的 `15.0s` 文案。

## 2. 错误传播链

```text
TcpClient.send_request(...)
-> CommandProtocol.dxf_prepare_plan(...) / dxf_preview_snapshot(...)
-> PreviewSnapshotWorker.completed(...)
-> MainWindow._on_preview_snapshot_completed(...)
-> PreviewSessionOwner.handle_worker_error(...)
-> "胶点预览生成失败" detail
```

## 3. 规则

1. transport 层 timeout 仍由 `TcpClient.send_request(...)` 统一生成。
2. timeout 文本必须使用本次请求实际传入的 timeout 数值。
3. worker、UI 和 preview session 不得再单独维护一个“15 秒”或“100 秒”的平行文案源。
4. 当错误属于非 timeout 失败时，UI 必须保留原始业务错误，不得强制改写为 timeout。

## 4. 可接受输出

对于本特性覆盖的 timeout 场景，以下输出是可接受的：

- `Request timed out (100.0s)`
- 语义等价、明确体现 100 秒上限的用户可见超时信息

以下输出一律不可接受：

- `Request timed out (15.0s)`
- 不体现真实等待上限的过期超时文案

## 5. 失败边界

出现以下任一情况时，本 contract 视为失败：

1. 本地请求已经等待到 `100s`，但 UI 仍显示 `15.0s`。
2. UI 自行拼接 timeout 秒数，导致与 transport 层实际值不一致。
3. 非 timeout 错误被统一展示成 timeout。

## 6. 与正式协议的关系

本 contract 描述的是 HMI 本地错误传播，不改变 runtime-gateway 响应 schema，也不新增共享协议字段。
