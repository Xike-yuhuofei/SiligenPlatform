# Phase 0 Research: HMI 在线点胶轨迹预览一致性验证

## Decision 1: 在线预览的硬前提冻结为 Supervisor 到达 `online_ready`

- Decision: 本特性的真实预览验收只覆盖 HMI 以 `online` 模式运行、Supervisor 成功经过 `backend_starting -> backend_ready -> tcp_connecting -> hardware_probing -> online_ready` 阶段序列，且 `online_ready=true` 的场景。只要未进入该状态，就不允许把当前结果记为真实在线预览通过。
- Rationale: 仓内 `apps/hmi-app/scripts/online-smoke.ps1` 的成功路径已经把 `online_ready=true` 和最小阶段序列固化为在线 GUI 契约。如果绕开这个前提，就无法证明当前预览属于真实机台已就绪的在线上下文。
- Alternatives considered:
  - 只要 HMI 进程处于 online 模式就允许验收: 拒绝。不能证明后端、TCP 和硬件探测已经到达可执行前状态。
  - 继续沿用旧的非在线验收口径: 拒绝。与已变更的需求冲突。

## Decision 2: 权威在线预览直接复用现有 `dxf.preview.snapshot -> runtime_snapshot` 链路

- Decision: 不再设计额外的本地权威提供者，也不引入新的预览来源标签。本特性的权威在线预览直接复用现有 runtime/gateway 链路：真实 DXF 进入 `dxf.artifact.create` / `dxf.plan.prepare` 后，由 `dxf.preview.snapshot` 返回 `runtime_snapshot`。
- Rationale: 仓内现有正式协议、HIL 脚本和验收清单都已把 `runtime_snapshot` 定义为真实在线预览的唯一可接受来源。继续扩展另一条“在线但非 runtime”的链路只会增加语义分叉和验收歧义。
- Alternatives considered:
  - 继续沿用旧的本地快照方案: 拒绝。与当前在线场景不符。
  - 复用 HTML 几何预览或本地 planner 结果作为在线权威来源: 拒绝。无法证明与当前 runtime 执行准备同源。

## Decision 3: 一致性比较对象以“准备下发到运动控制卡的数据语义”冻结

- Decision: “预览与上位机下发到运动控制卡的数据一致”的判定口径，统一冻结为执行准备语义一致，而不是字节格式一致。比较维度限定为几何路径、路径顺序和点胶相关运动语义，并通过 `plan_id`、`plan_fingerprint`、`snapshot_hash` 等关联信息证明它们来自同一执行准备上下文。
- Rationale: 用户已经明确说明格式可以不同，因此比较目标只能是业务语义，而不是传输格式逐字段相等。仓内协议也已表明预览快照与执行准备存在不同响应结构，但必须共用同一 plan 上下文。
- Alternatives considered:
  - 要求预览结果与控制卡下发内容逐字节一致: 拒绝。与“格式可以不同”的补充条件冲突。
  - 只比较 HMI 绘制轮廓是否看起来像 DXF: 拒绝。无法覆盖顺序和点胶运动语义。

## Decision 4: 来源分类保持简单，真实验收只接受 `runtime_snapshot`

- Decision: 在线真实预览验收只接受 `preview_source == runtime_snapshot`。`mock_synthetic`、来源缺失、来源不明、历史缓存或与当前 DXF / 当前 plan 无法回链的结果，一律视为非通过。
- Rationale: 仓内 `docs/validation/dispense-trajectory-preview-real-acceptance-checklist.md` 已把 `runtime_snapshot` 冻结为真实 runtime 轨迹预览的唯一可接受来源，`run_real_dxf_preview_snapshot.py` 也会在来源偏离时直接失败。
- Alternatives considered:
  - 只要不是 `mock_synthetic` 就允许通过: 拒绝。来源边界过宽，无法审计。
  - 引入新的在线来源枚举供本特性使用: 拒绝。没有事实依据，会增加契约漂移。

## Decision 5: `samples/dxf/rect_diag.dxf` 继续作为强制真实 DXF 基线

- Decision: 本特性的强制自动化基线继续使用 `D:\Projects\SiligenSuite\samples\dxf\rect_diag.dxf`，并复用 `tests/baselines/preview/rect_diag.preview-snapshot-baseline.json`、`tests/e2e/first-layer/test_real_preview_snapshot_geometry.py` 和 `tests/e2e/hardware-in-loop/run_real_dxf_preview_snapshot.py`。
- Rationale: 这组资产已经对 `runtime_snapshot` 的点数、范围、关键采样点和几何摘要形成闭环，是当前仓内最稳定、最可追溯的真实 DXF 预览基线。
- Alternatives considered:
  - 依赖现场临时 DXF 作为唯一验收输入: 拒绝。不可复现、不可审计。
  - 使用 synthetic / mock 几何替代真实 DXF: 拒绝。与需求冲突。

## Decision 6: 验证策略采用“根级入口 + 在线 smoke + 契约/几何/HIL”组合

- Decision: 本特性的最小验证组合固定为根级 `.\build.ps1` / `.\test.ps1` 的显式子集，加上 `apps/hmi-app\scripts\online-smoke.ps1`、`pytest tests/contracts/test_protocol_compatibility.py -q`、`pytest tests/e2e/first-layer/test_real_preview_snapshot_geometry.py -q`，以及面向真实机台就绪上下文的 `python tests/e2e/hardware-in-loop/run_real_dxf_preview_snapshot.py --config-mode real`。
- Rationale: 在线 smoke 证明 HMI 到达 `online_ready`；协议测试保证 `preview_source` 和字段契约不漂移；first-layer 几何回归保证 canonical DXF 的形状稳定；HIL 快照脚本负责生成真实运行证据并校验 `runtime_snapshot`。
- Alternatives considered:
  - 只跑 HMI GUI smoke: 拒绝。无法证明协议字段和几何基线保持一致。
  - 只跑 HIL 脚本: 拒绝。无法覆盖 HMI 在线就绪门禁。
