# HMI Supervisor 最小契约说明 V1

更新时间：`2026-03-20`

## 1. 目标与范围

本文档定义方案 B 的 P0 落地契约，目标是把 online 启动链路的“就绪真相”从 HMI 进程内逻辑外置为 Supervisor 的显式状态。

本阶段只解决：

1. 启动链路状态外置。
2. 失败可诊断。
3. 恢复动作可执行。
4. HMI 能力门禁统一。
5. smoke 可回归。

本阶段不包含：

1. 通用 IPC 框架抽象。
2. 多客户端共享会话。
3. 会话历史持久化。
4. 服务总线化扩展。
5. HMI 大规模结构重构。

补充说明：

- 关于运行态 `raw_io`、`effective_interlocks` 与 `supervision` 的边界划分，见 `docs/architecture/topics/runtime-hmi/runtime-supervision-state-contract-boundary-v1.md`。
- 当前设计基线已采纳：`effective_interlocks` 作为正式对外契约暴露，`supervision` 显式提供 `requested_state` 与 `state_change_in_process`，并以 `door`、`negative-limit` 作为第一批样板场景。

## 2. 术语与权威边界

- `Supervisor`：本机启动链路编排者，负责 backend/TCP/hardware 生命周期与状态发布。
- `HMI`：状态消费者与能力门禁执行者，不拥有 online 就绪真相解释权。
- `online_ready`：唯一在线可操作判定，来自 Supervisor 显式状态。

强约束：

- HMI 不得再通过“TCP 连接成功”或“backend 进程存活”等单点信号推断 ready。
- HMI 只能基于 Supervisor 契约做联机状态展示与门禁判定。

## 3. 最小状态契约（V1）

### 3.1 顶层对象

Supervisor 必须对外暴露统一快照对象 `SessionSnapshot`：

```json
{
  "mode": "offline | online",
  "session_state": "idle | starting | ready | degraded | failed | stopping",
  "backend_state": "stopped | starting | ready | failed",
  "tcp_state": "disconnected | connecting | ready | failed",
  "hardware_state": "unavailable | probing | ready | failed",
  "failure_code": "string | null",
  "failure_stage": "string | null",
  "recoverable": true,
  "last_error_message": "string | null",
  "updated_at": "ISO-8601"
}
```

补充约束：

1. `offline` 路径同样必须通过 `SessionSnapshot` 对外表达，不得在 HMI 内部额外构造“无快照 launch result”作为替代真相。
2. `LaunchResult`、UI label state、按钮状态仅允许作为 `SessionSnapshot` 的派生结果，不得成为并行真值源。

### 3.2 字段约束

1. `mode`：
- `offline`：明确不尝试 gateway 启动、TCP 连接、硬件探测。
- `online`：执行完整启动链路。

2. `session_state`：
- `idle`：尚未启动或已停止。
- `starting`：启动链路进行中。
- `ready`：全链路通过，可进入在线操作。
- `degraded`：部分能力不可用（P0 可仅用于保留态，不强制进入）。
- `failed`：启动或运行失败，需要恢复动作。
- `stopping`：主动停止进行中。

3. `failure_code` 与 `failure_stage`：
- `session_state=failed` 时必须非空。
- 非 `failed` 时必须为 `null`。

4. `recoverable`：
- 指示是否允许执行“重试/重连/重启 backend”等恢复动作。

## 4. 启动状态机（P0）

## 4.1 状态阶段

online 启动阶段固定为：

1. `backend_starting`
2. `backend_ready`
3. `tcp_connecting`
4. `tcp_ready`
5. `hardware_probing`
6. `hardware_ready`
7. `online_ready`

### 4.2 进入、成功、超时、失败

1. `backend_starting`
- 进入条件：`mode=online` 且收到启动请求。
- 成功条件：backend 进程被成功拉起或确认已存在可用实例。
- 失败条件：可执行文件缺失、拉起失败、启动被禁用。

2. `backend_ready`
- 进入条件：`backend_starting` 成功。
- 成功条件：在超时窗口内通过 ready 探测。
- 超时条件：超过配置超时仍未 ready。

3. `tcp_connecting`
- 进入条件：`backend_ready` 成功。
- 成功条件：TCP 建连成功。
- 失败条件：连接拒绝、连接超时、协议握手失败。

4. `hardware_probing`
- 进入条件：`tcp_ready` 成功。
- 成功条件：硬件 connect/probe 命令返回就绪。
- 失败条件：硬件不可达、初始化失败。

5. `online_ready`
- 进入条件：`hardware_ready` 成功。
- 成功条件：Supervisor 快照满足 `online_ready` 判定。

## 5. Ready 真相判定

`online_ready` 必须满足以下布尔合取：

1. `mode == online`
2. `session_state == ready`
3. `backend_state == ready`
4. `tcp_state == ready`
5. `hardware_state == ready`
6. `failure_code == null`

任一条件不满足即非 `online_ready`。

## 6. 错误模型（P0）

### 6.1 失败码集合

P0 固定失败码：

- `SUP_BACKEND_AUTOSTART_DISABLED`
- `SUP_BACKEND_SPEC_MISSING`
- `SUP_BACKEND_EXE_NOT_FOUND`
- `SUP_BACKEND_START_FAILED`
- `SUP_BACKEND_READY_TIMEOUT`
- `SUP_TCP_CONNECT_FAILED`
- `SUP_HARDWARE_CONNECT_FAILED`
- `SUP_BACKEND_EXITED`
- `SUP_UNKNOWN`

### 6.2 失败码规则

1. 每次进入 `failed` 必须绑定单一 `failure_code` 与 `failure_stage`。
2. `last_error_message` 用于保留人类可读诊断，不替代 `failure_code`。
3. 相同根因必须复用同一失败码，禁止 UI 层拼接新错误语义。

## 7. 恢复动作契约（P0）

Supervisor 至少支持以下恢复动作语义：

1. `retry_stage`
- 对当前失败阶段重试一次。

2. `restart_session`
- 清理当前状态并从 `backend_starting` 重启 online 链路。

3. `stop_session`
- 进入 `stopping`，最终回到 `idle`。

恢复动作执行后必须更新 `SessionSnapshot`，并在失败时刷新 `failure_code/failure_stage/last_error_message`。

恢复动作边界补充约束：

1. `retry_stage` 与 `restart_session` 必须由 Supervisor 自身校验 `mode=online`、`session_state=failed`、`recoverable=true` 后才允许执行。
2. `stop_session` 必须由 Supervisor 自身校验当前会话处于 `ready` 或 `failed` 后才允许执行。
3. UI 层按钮禁用或提示文案只能作为外层防呆，不能替代 Supervisor 对恢复动作合法性的最终校验。

## 8. HMI 消费与能力门禁

### 8.1 消费规则

1. HMI 只读 Supervisor 快照，不直接推导 backend/TCP/hardware 真相。
2. HMI 顶部状态区、按钮可用性、错误弹窗均以 `SessionSnapshot` 为唯一输入。
3. HMI 在首个可解析 `SessionSnapshot` 到达前，必须呈现非 ready 观测；不得预设“系统就绪”“空闲”“在线已完成”等文案。

### 8.2 门禁规则

1. 仅在 `online_ready=true` 时允许在线能力。
2. `starting`、`degraded`、`failed`、`stopping` 一律视为不可在线操作。
3. `offline` 模式下，所有联机能力必须统一返回“Offline 模式不可用”。

### 8.3 在线能力最小集合（P0）

以下能力必须受统一门禁控制：

- 回零（含单轴与全轴）
- 点动
- 生产开始/暂停/恢复/停止
- DXF 加载与执行
- 点胶与供料控制
- 告警确认/清除

## 9. smoke 与退出码映射

现有契约保持不变：

- `offline-smoke.ps1`：`0/10/11`
- `online-smoke.ps1`：`0/10/11/20/21`

P0 新要求：

1. `online-smoke` 判定基于 Supervisor 快照，不再基于 UI 内部隐式状态。
2. 当 `online-smoke` 返回 `20/21` 时，必须能从诊断输出中定位 `failure_code/failure_stage`。
3. `verify-online-ready-timeout.ps1` 触发超时时，`failure_code` 必须映射为 `SUP_BACKEND_READY_TIMEOUT`。

## 10. 可观测性最小要求

Supervisor 必须输出最小诊断事件（可写日志或结构化输出）：

1. 阶段进入事件：`stage_entered`。
2. 阶段成功事件：`stage_succeeded`。
3. 阶段失败事件：`stage_failed`（含 `failure_code`）。
4. 恢复动作事件：`recovery_invoked`。

要求：每条事件必须包含 `timestamp`、`session_id`、`stage`。

## 11. 验收标准（P0）

满足以下条件即视为 P0 契约落地完成：

1. HMI 在线状态与能力门禁完全由 Supervisor 快照驱动。
2. `online_ready` 判定唯一且可重复验证。
3. `online-smoke` 与 `verify-online-ready-timeout` 可稳定回归，退出码不变。
4. 任意启动失败都可定位到固定 `failure_code/failure_stage`。
5. offline 与 online 均通过统一 `SessionSnapshot` 对外表达。
6. 恢复动作的非法状态跃迁由 Supervisor owner 层显式拒绝，而不是仅靠 UI 边界层拦截。
7. 未引入方案 C 范围内的新平台化抽象。
