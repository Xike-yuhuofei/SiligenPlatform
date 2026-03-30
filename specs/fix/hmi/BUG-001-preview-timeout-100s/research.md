# Phase 0 Research: HMI 胶点预览超时窗口调整

## Decision 1: `300s` 属于 HMI 本地等待策略，不属于共享 DXF wire contract

- Decision: 本特性的 `300s` 超时预算定义为 HMI 客户端对请求响应的本地等待上限，owner surface 固定在 `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\client\tcp_client.py`、`D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\client\protocol.py` 和 `D:\Projects\SiligenSuite\modules\hmi-application\application\hmi_application\preview_session.py` 组成的本地链路，不向 `D:\Projects\SiligenSuite\shared\contracts\application\commands\dxf.command-set.json` 新增 timeout 字段。
- Rationale: 当前 `dxf.command-set.json` 只定义 `dxf.plan.prepare` / `dxf.preview.snapshot` 的 payload schema，并没有客户端等待时长字段；`TcpClient.send_request(...)` 通过 `Queue.get(timeout=timeout)` 在本地决定超时。用户报错里的 `Request timed out (15.0s)` 也是本地 transport 文案，而不是服务端协议字段。
- Alternatives considered:
  - 在共享 `dxf.command-set.json` 中新增超时参数: 拒绝。会把本地等待策略误升级为跨组件协议变更，扩大影响面。
  - 在 runtime-gateway 侧统一修改默认处理时间: 拒绝。用户需求聚焦于 HMI 过早失败，不要求改变后端执行模型。

## Decision 2: `300s` 预算必须同时覆盖 `dxf.plan.prepare` 与 `dxf.preview.snapshot`

- Decision: 对“打开 DXF 自动触发的胶点预览”这条链路，`PreviewSnapshotWorker.run()` 内部串行调用的 `dxf.plan.prepare` 和 `dxf.preview.snapshot` 都必须使用 `300s` 超时预算。
- Rationale: 真实复杂 DXF 在 mock/TCP 复测链路下，`plan.prepare` 可稳定超过 100 秒，因此预算需要一次性提升到 300 秒并与复测窗口对齐。当前 `PreviewSnapshotWorker` 会先调用 `protocol.dxf_prepare_plan(...)`，成功后再调用 `protocol.dxf_preview_snapshot(...)`。两者在 `CommandProtocol` 中都硬编码为 `15.0s`；无论超时发生在 prepare 还是 snapshot，用户最后看到的都是同一条“胶点预览生成失败”链路。如果只放宽其中一个阶段，另一阶段仍会在 `15.0s` 处把整条用户流程打断。
- Alternatives considered:
  - 只修改 `dxf.preview.snapshot`: 拒绝。`dxf.plan.prepare` 仍可能成为 15 秒瓶颈。
  - 只修改 `dxf.plan.prepare`: 拒绝。真正的 snapshot 请求仍会在 15 秒失败。

## Decision 3: 采用“调用点显式 override”，而不是全局替换所有 DXF 默认超时

- Decision: 通过 `CommandProtocol` 为 preview 相关方法提供显式 timeout override，并只在 `PreviewSnapshotWorker` 这条“打开 DXF 自动预览”链路上传入 `300s`；保留 runtime resync、`dxf.preview.confirm`、`dxf.job.start`、`connect`、`dxf.load` 等其他入口的现有默认超时。
- Rationale: `dxf_preview_snapshot_with_error_details(...)` 不只被 worker 使用，也被 `main_window._sync_preview_state_from_runtime()` 用于 resync。如果把默认值统一改成 `300.0s`，就会悄悄扩大到运行时同步与其他非目标入口，违反当前 spec 的边界。显式 override 可以把 `300s` 精确限制在“打开 DXF 自动预览”这一条链路。
- Alternatives considered:
  - 直接把 `CommandProtocol` 中全部 `15.0s` 改成 `300.0s`: 拒绝。会误伤与本需求无关的入口。
  - 在 `TcpClient.send_request(...)` 内按 method 名硬编码特殊规则: 拒绝。会把场景边界埋在底层 transport，后续难以审计和测试。

## Decision 4: 用户可见超时信息继续直接继承 transport 层错误文本

- Decision: 用户可见的超时报错不新增独立 UI 文案源；继续由 `TcpClient.send_request(...)` 根据实际 timeout 生成 `Request timed out ({timeout}s)`，再经 `PreviewSnapshotWorker -> MainWindow._on_preview_snapshot_completed(...) -> PreviewSessionOwner.handle_worker_error(...)` 原样传播到“胶点预览生成失败”详情区域。
- Rationale: 当前 `main_window.py` 在 worker 失败时直接把 `error` 作为 detail 文案写入 UI；`TcpClient` 已经把 timeout 变量带入错误字符串。只要 worker 路径上的 timeout 实参变为 `300.0`，UI 会自然显示 `300.0s` 或等价的 300 秒信息，不需要再在 UI 层复制一套秒数常量。
- Alternatives considered:
  - 在 `main_window.py` 单独写死“300 秒超时”: 拒绝。会与实际 transport timeout 脱节，未来再次修改时容易失真。
  - 对所有 timeout 错误统一隐藏具体秒数: 拒绝。已澄清的需求要求用户可见提示明确反映新阈值。

## Decision 5: 回归锁定在 Python 协议与 UI 层，不新增 shared contract 兼容测试任务

- Decision: 本特性的最小回归集聚焦在 `D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\test_protocol_preview_gate_contract.py` 和 `D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\test_main_window.py`；前者锁定 timeout 传参与 protocol 边界，后者锁定用户可见失败表现。
- Rationale: 这次没有新增 shared JSON schema 字段，也不改变 runtime-gateway 的 wire payload shape，正式协议契约测试不是首要回归面；真正的风险在于 HMI 是否仍在错误链路上保留 `15.0s`，或把 `300s` 扩散到不该扩散的入口。
- Alternatives considered:
  - 只依赖手工打开 DXF 验证: 拒绝。无法稳定锁定 call-site 级 timeout 传参与 UI 文案。
  - 把 shared contract 兼容测试作为唯一回归: 拒绝。它不能证明 HMI 本地等待预算是否正确。
