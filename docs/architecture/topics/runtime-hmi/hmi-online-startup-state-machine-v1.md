# HMI Online 启动链路状态机说明 V1

更新时间：`2026-03-20`

## 1. 目标与适用范围

本文档定义方案 B 的 P0 online 启动链路状态机，作为 `docs/architecture/topics/runtime-hmi/hmi-supervisor-min-contract-v1.md` 的执行级补充。

适用范围：

1. Supervisor 侧的 online 启动与失败恢复编排。
2. HMI 侧的状态消费、能力门禁与可观测点。
3. `apps/hmi-app` 现有 smoke 契约下的观察与判定口径。

不包含：

1. IPC 传输形态选型。
2. 多会话/多客户端并发调度。
3. 持久化历史与审计存档扩展。

## 2. 与最小契约说明 V1 的一致性约束

本文件遵循 `hmi-supervisor-min-contract-v1.md`，并以其为上位约束。若出现语义冲突，以最小契约说明 V1 为准。

固定一致项：

1. `online_ready` 只能由 Supervisor `SessionSnapshot` 判定。
2. 启动阶段固定为：
- `backend_starting`
- `backend_ready`
- `tcp_connecting`
- `tcp_ready`
- `hardware_probing`
- `hardware_ready`
- `online_ready`
3. 失败码集合固定为 V1 定义的 9 个 `SUP_*` 码。

## 3. 状态机总览

## 3.1 顶层会话状态

`session_state` 取值：`idle | starting | ready | degraded | failed | stopping`

P0 推荐转移：

1. `idle -> starting`
2. `starting -> ready`
3. `starting -> failed`
4. `ready -> failed`
5. `failed -> starting`（恢复动作触发）
6. `ready -> stopping -> idle`
7. `failed -> stopping -> idle`

### 3.2 online_ready 唯一判定

仅当以下条件同时成立时，系统进入 `online_ready`：

1. `mode == online`
2. `session_state == ready`
3. `backend_state == ready`
4. `tcp_state == ready`
5. `hardware_state == ready`
6. `failure_code == null`

任何条件不满足，均视为非 `online_ready`。

## 4. 分阶段冻结定义

说明：以下超时口径用于状态机判定。`N/A` 表示该阶段不设置独立超时。

| 阶段 | 进入条件 | 成功条件 | 超时条件 | 失败条件 | failure_code 映射 | recoverable 判定 | 恢复动作与回退规则 |
|---|---|---|---|---|---|---|---|
| `backend_starting` | `mode=online` 且收到 `start_session`；`session_state=idle/failed` | backend 进程拉起成功，或确认已有可用 backend 实例 | `N/A` | 自动拉起被禁用；启动契约缺失；可执行文件缺失；创建进程失败 | `SUP_BACKEND_AUTOSTART_DISABLED` / `SUP_BACKEND_SPEC_MISSING` / `SUP_BACKEND_EXE_NOT_FOUND` / `SUP_BACKEND_START_FAILED` | `AUTOSTART_DISABLED/SPEC_MISSING/EXE_NOT_FOUND` 默认不可恢复；`START_FAILED` 可恢复 | 不可恢复：保持 `failed`，仅允许 `stop_session`；可恢复：允许 `retry_stage`，失败两次后建议 `restart_session` |
| `backend_ready` | `backend_starting` 成功；`backend_state=starting` | 在就绪窗口内探测到 backend 可连接 | 超过 `5s` 仍不可连接 | backend 在等待期提前退出；就绪超时 | `SUP_BACKEND_EXITED` / `SUP_BACKEND_READY_TIMEOUT` | 可恢复 | 首选 `retry_stage`（重探测）；连续失败后 `restart_session`；仍失败则停留 `failed` |
| `tcp_connecting` | `backend_ready` 成功 | TCP 建连成功并更新 `tcp_state=ready` | 超过 `3s` 未建连成功 | 连接拒绝/连接超时/握手失败 | `SUP_TCP_CONNECT_FAILED` | 可恢复 | 首选 `retry_stage`（重连）；重连上限后 `restart_session`；必要时 `stop_session` 回 `idle` |
| `tcp_ready` | `tcp_connecting` 成功 | `tcp_state=ready` 且未观测到断连 | `N/A` | 连接建立后立即断开或协议不可用 | `SUP_TCP_CONNECT_FAILED` / `SUP_UNKNOWN` | 可恢复 | 立即回退到 `tcp_connecting` 执行 `retry_stage`；多次失败升级 `failed` |
| `hardware_probing` | `tcp_ready` 成功 | `connect_hardware/probe` 返回已就绪 | 超过 `15s` 仍未就绪 | 硬件连接失败、探测失败、后端返回硬件不可达 | `SUP_HARDWARE_CONNECT_FAILED` | 可恢复 | 首选 `retry_stage`；重试上限后 `restart_session`；若用户切换离线则 `stop_session` |
| `hardware_ready` | `hardware_probing` 成功 | `hardware_state=ready` 且保持稳定 | `N/A` | 刚进入 ready 即丢失硬件可用性 | `SUP_HARDWARE_CONNECT_FAILED` / `SUP_UNKNOWN` | 可恢复 | 回退到 `hardware_probing` 重试；持续失败升级 `failed` |
| `online_ready` | `hardware_ready` 成功且满足 3.2 条件 | `session_state=ready` 并持续满足 `online_ready` 布尔合取 | `N/A` | 任一子状态退化；出现任何 `failure_code` | 沿用触发退化的 failure_code | 通常可恢复（取决于 failure_code） | 立即退出 `online_ready`，进入对应失败阶段或 `failed`；HMI 同步关闭在线能力 |

## 5. failure_stage 映射规则

进入 `failed` 时，`failure_stage` 必须为以下枚举之一：

1. `backend_starting`
2. `backend_ready`
3. `tcp_connecting`
4. `tcp_ready`
5. `hardware_probing`
6. `hardware_ready`
7. `online_ready`

映射约束：

1. 同一次失败只允许一个 `failure_stage`。
2. `failure_stage` 必须是“首次失败发生点”，不得被后续连带错误覆盖。

## 6. recoverable 与不可恢复判定

## 6.1 判定原则

1. 若无需外部工件修复，仅通过 Supervisor 动作可继续推进，则 `recoverable=true`。
2. 需要外部修复（路径、可执行文件、配置契约缺失）则 `recoverable=false`。

### 6.2 P0 默认判定表

- `SUP_BACKEND_AUTOSTART_DISABLED`：`recoverable=false`
- `SUP_BACKEND_SPEC_MISSING`：`recoverable=false`
- `SUP_BACKEND_EXE_NOT_FOUND`：`recoverable=false`
- `SUP_BACKEND_START_FAILED`：`recoverable=true`
- `SUP_BACKEND_READY_TIMEOUT`：`recoverable=true`
- `SUP_TCP_CONNECT_FAILED`：`recoverable=true`
- `SUP_HARDWARE_CONNECT_FAILED`：`recoverable=true`
- `SUP_BACKEND_EXITED`：`recoverable=true`
- `SUP_UNKNOWN`：默认 `recoverable=false`（除非运行时策略显式判定可恢复）

## 7. 恢复动作与回退规则

P0 仅允许以下动作：

1. `retry_stage`
- 仅重试当前失败阶段。
- 不重置上游成功阶段的状态快照。

2. `restart_session`
- 清理 backend/TCP/hardware 临时态。
- 统一回退到 `backend_starting` 从头启动。

3. `stop_session`
- 进入 `stopping`，完成资源释放后转 `idle`。

回退强约束：

1. 任何恢复动作执行前，必须先退出 `online_ready`。
2. `recoverable=false` 时禁止自动 `retry_stage` 循环。
3. 单阶段重试达到上限后，不得继续隐式重试，必须显式转 `failed` 并暴露恢复动作入口。
4. `retry_stage` 与 `restart_session` 仅允许在 `mode=online`、`session_state=failed`、`recoverable=true` 时执行。
5. `stop_session` 仅允许在 `mode=online` 且 `session_state in {ready, failed}` 时执行。
6. 上述合法性必须由 Supervisor owner 层自身校验；HMI 禁用按钮不能替代该校验。

## 8. HMI 可观测点定义

HMI 必须消费并展示以下 Supervisor 观测字段：

1. `mode`
2. `session_state`
3. `backend_state`
4. `tcp_state`
5. `hardware_state`
6. `failure_code`
7. `failure_stage`
8. `recoverable`
9. `last_error_message`
10. `updated_at`

展示与门禁强约束：

1. 顶部模式与状态文案仅由快照字段生成。
2. 在线按钮可用性仅由 `online_ready` 结果控制。
3. `session_state != ready` 或 `failure_code != null` 时，在线按钮必须全部禁用。
4. HMI 不得使用本地 TCP socket 状态覆盖 Supervisor 状态。
5. 在首个可解析快照到达前，HMI 必须显示非 ready 初始态，不得默认显示“系统就绪”“空闲”或其他完成态文案。

## 9. smoke 对应观察点

## 9.1 offline smoke

保持现有退出码语义：`0/10/11`。

观察点：

1. `mode=offline`
2. `session_state` 不进入 `starting/ready`
3. 在线能力门禁全部关闭

### 9.2 online smoke

保持现有退出码语义：`0/10/11/20/21`。

观察点：

1. 成功路径：阶段按序到达 `online_ready`。
2. `20`（mock 启动失败）：应出现 `failure_stage=backend_starting`，`failure_code` 属于 backend 启动失败族。
3. `21`（mock ready 超时）：应出现 `failure_stage=backend_ready`，`failure_code=SUP_BACKEND_READY_TIMEOUT`。
4. `10/11`（GUI 失败/超时）：Supervisor 状态必须可被脚本采样，不能只依赖 UI 文案。

## 10. “假在线”消除条款（强制）

以下任一情况必须判定为非在线：

1. `session_state != ready`
2. `failure_code != null`
3. `backend_state/tcp_state/hardware_state` 任一不为 `ready`
4. HMI 未拿到可解析的最新快照

禁止行为：

1. HMI 以“TCP 已连接”替代 `online_ready`。
2. HMI 以“backend 进程仍在”替代 `online_ready`。
3. HMI 在 `starting/degraded/failed/stopping` 阶段放行在线操作。
4. HMI 在启动空窗期以默认文案显示“系统就绪”“空闲”或其他 ready/complete 语义。

## 11. 验收判定

状态机文档冻结完成的判定标准：

1. 每个阶段的进入/成功/超时/失败/failure_code/recoverable/恢复动作均已定义。
2. `failure_stage` 与 `failure_code` 映射可用于脚本诊断。
3. smoke 观察点与退出码口径一致且不改变现有退出码。
4. 文档未引入超出 P0 的平台化扩展范围。
