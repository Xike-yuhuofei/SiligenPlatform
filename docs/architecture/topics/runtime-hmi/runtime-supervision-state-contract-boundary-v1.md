# 运行态监督状态契约边界说明 V1

更新时间：`2026-03-26`

## 1. 目标与范围

本文档用于补齐当前运行态状态契约中的三个边界：

1. `raw_io`：原始输入信号镜像。
2. `effective_boundary / effective_interlock`：控制器当前生效的保护边界与联锁事实。
3. `supervision state`：HMI 与 smoke 用于能力门禁、就绪判定、错误诊断的监督态。

本文档只处理状态契约边界，不处理：

1. 实际协议字段落地实现。
2. HMI 页面重构。
3. 真实机台安全回路设计。
4. PackML 全量引入。

## 2. 背景问题

本轮无机台回归已暴露出三个稳定问题：

1. `door` 场景中，`mock.io.set(door=...)` 后第一帧 `status` 可能滞后，必须轮询到 `machine_state=Fault/Idle` 收敛后才能下结论。
2. `negative-limit` 场景中，`mock.io.set(limit_x_neg/limit_y_neg=true)` 会驱动 HOME boundary，并阻断负向运动，但 `status.io.limit_x_neg/limit_y_neg` 不暴露该有效边界。
3. `HMI online smoke` 当前验证的是“链路能走通”，而不是“真实机台运动和安全链已放行”。

以上三个问题的共同根因不是脚本偶发错误，而是当前状态对象把“原始信号”“生效保护”“监督门禁”混在同一观察面中。

## 3. 设计结论

本项目下一阶段的目标状态契约采用“双通道状态模型 + 监督态分层”：

1. 保留 `raw_io` 作为原始输入镜像，不强行承载控制器推导后的保护语义。
2. 单独暴露 `effective_interlocks` 或 `effective_boundaries`，表达当前真实生效的门禁、限位、边界、脱困约束。
3. 单独暴露 `supervision`，表达 HMI 和 smoke 需要消费的监督态、阶段迁移中状态、失败原因与门禁真相。

结论性约束：

1. 不建议把 HOME boundary 重新映射回 `raw_io.limit_x_neg/limit_y_neg`。
2. 建议把 HOME boundary 作为 `effective_boundary` 暴露，而不是复用原始 I/O 字段。
3. HMI 与 smoke 的权威门禁判断应基于 `supervision + effective_interlocks`，而不是直接基于第一帧 `raw_io`。
4. `effective_interlocks` 采纳为正式对外契约的一部分，而不是仅作为内部诊断对象。
5. `supervision` 采纳显式 `requested_state` 与 `state_change_in_process`，用于表达“请求已发出”和“事实已达成”之间的过渡窗口。
6. `door` 与 `negative-limit` 采纳为第一批样板场景，作为状态契约落地和回归口径统一的优先对象。

## 4. 分层职责

### 4.1 `raw_io`

定义：设备输入层或 mock 输入层的镜像值。

职责：

1. 反映当前读取到的原始输入位。
2. 保持与硬件接线或 mock 注入动作的一致性。
3. 为诊断提供“输入源头”证据。

不承载：

1. 控制器内部推导的软边界。
2. 运动脱困规则。
3. HMI 的在线放行真相。

示例：

- `door`
- `estop`
- `limit_x_neg`
- `limit_y_neg`

### 4.2 `effective_interlocks / effective_boundaries`

定义：控制器当前真实生效的联锁和边界事实。

职责：

1. 表达当前哪些保护规则已经在运行态生效。
2. 表达该保护是“禁止全部动作”还是“仅允许逃逸方向动作”。
3. 表达保护语义来源于哪里，是原始输入、控制器推导、报警锁存还是内部状态机。

建议最小字段：

1. `door_open_active`
2. `estop_active`
3. `home_boundary_x_active`
4. `home_boundary_y_active`
5. `positive_escape_only_axes`
6. `source`

`source` 不是为了给 UI 看，而是为了避免未来再次把“原始输入为假”误判成“保护未生效”。

### 4.3 `supervision`

定义：面向 HMI、smoke、诊断和能力门禁的监督态快照。

职责：

1. 提供当前权威运行态。
2. 提供请求态与迁移中的显式标记。
3. 提供错误原因、失败阶段、可恢复性。
4. 为 HMI 在线按钮门禁提供唯一真相。

建议最小字段：

1. `current_state`
2. `requested_state`
3. `state_change_in_process`
4. `state_reason`
5. `failure_code`
6. `failure_stage`
7. `recoverable`
8. `updated_at`

## 5. 当前问题的契约归位

### 5.1 `door` 首帧滞后

归位方式：

1. `raw_io.door` 只表示本帧读到的门输入状态。
2. `effective_interlocks.door_open_active` 表示门禁保护当前是否已经真正生效。
3. `supervision.current_state/state_reason` 表示当前监督态是否已经进入 `Fault/interlock_door_open` 或恢复到 `Idle`。

对测试与 HMI 的影响：

1. 不再对第一帧 `raw_io.door` 直接做最终门禁结论。
2. 最终门禁结论以 `effective_interlocks + supervision` 收敛结果为准。

### 5.2 `negative-limit` 与 HOME boundary

归位方式：

1. `raw_io.limit_x_neg/limit_y_neg` 保持“原始限位输入”的语义。
2. `effective_interlocks.home_boundary_x_active/home_boundary_y_active` 表示当前已进入 HOME boundary。
3. `effective_interlocks.positive_escape_only_axes` 表示当前仅允许哪条轴向正向逃逸。

设计约束：

1. 若 HOME boundary 是内部控制逻辑推导结果，就不应强行写回 `raw_io.limit_*`。
2. 若未来确有现场需求暴露“物理负限位已触发”，也应通过单独的原始信号字段表达，而不是把控制器边界和物理输入混为一谈。

### 5.3 `HMI online smoke` 边界

归位方式：

1. `online_ready` 仍由 Supervisor 快照负责。
2. 动作是否允许执行，除 `online_ready` 外，还应允许消费 `effective_interlocks` 的摘要字段。
3. smoke 验证结果定义为“监督链路与 mock 执行动作可达”，不升级为真实机台放行结论。

## 6. 建议的最小对象形态

以下为建议结构，不代表最终字段名必须一字不差：

```json
{
  "supervision": {
    "current_state": "Idle | Fault | Running | Paused",
    "requested_state": "string | null",
    "state_change_in_process": false,
    "state_reason": "string | null",
    "failure_code": "string | null",
    "failure_stage": "string | null",
    "recoverable": true,
    "updated_at": "ISO-8601"
  },
  "raw_io": {
    "door": false,
    "estop": false,
    "limit_x_neg": false,
    "limit_y_neg": false
  },
  "effective_interlocks": {
    "door_open_active": false,
    "estop_active": false,
    "home_boundary_x_active": false,
    "home_boundary_y_active": false,
    "positive_escape_only_axes": [],
    "source": []
  }
}
```

## 7. 迁移策略

本次设计建议采用“目标契约明确、消费者渐进迁移”的方式。

### 7.1 过渡原则

1. 先新增 `effective_interlocks` 与 `supervision` 语义，不立即改变 `raw_io` 既有含义。
2. 先让测试和 HMI 迁到新语义层，再考虑是否缩减旧断言口径。
3. 在消费者完成迁移前，不对 `raw_io.limit_*` 做破坏性重定义。

### 7.2 过渡收益

1. 不会把当前 `status.io` 消费方一次性打断。
2. 能先解决 door 首帧滞后和 HOME boundary 诊断不足的问题。
3. 能把“mock 可达”与“真实机台放行”边界写成正式口径。

## 8. 影响面

设计落地时，至少会影响以下面：

1. runtime gateway 状态返回结构。
2. HMI 状态显示与按钮门禁判定。
3. first-layer 回归脚本断言口径。
4. online smoke 的验收定义。
5. 文档中的状态机、验收基线与诊断说明。

## 9. 不建议的做法

以下做法理论上能掩盖眼前问题，但不建议作为正式方向：

1. 继续把所有门禁和边界语义都塞进 `status.io`。
2. 为了让脚本好断言，把 HOME boundary 直接伪装成 `raw_io.limit_*`。
3. 把 `online smoke` 的通过等同为真实机台运行放行。

## 10. 本轮设计裁决

### 10.1 `effective_interlocks` 作为正式对外契约暴露

裁决：采纳。

原因：

1. 当前 `door` 与 `negative-limit` 已证明“控制器真实生效保护”无法稳定从 `raw_io` 直接推断。
2. 若不把该层作为正式契约暴露，HMI、smoke 与诊断仍会继续争夺“谁来解释门禁真相”。
3. 该层既能服务 UI 门禁，也能服务自动化断言和现场诊断，收益高于新增字段成本。

### 10.2 `supervision` 显式提供 `requested_state` 与 `state_change_in_process`

裁决：采纳。

原因：

1. `door` 首帧滞后已经说明“状态迁移中”必须是一等语义，而不是消费者自行猜测。
2. HMI 若没有 `requested_state`，只能用按钮点击后的本地临时状态冒充真实过渡态，这与当前 Supervisor 设计目标冲突。
3. 这两个字段会显著降低 smoke、HMI 和日志对第一帧瞬态的误判概率。

### 10.3 `door` 与 `negative-limit` 作为第一批样板场景

裁决：采纳。

原因：

1. 两者已经有稳定复现实验和正式证据。
2. 两者正好覆盖“禁止全部动作”和“仅允许逃逸方向动作”两类典型保护语义。
3. 两者跨越 `raw_io`、`effective_interlocks`、`supervision` 三层，足够验证新契约是否真正闭合。

## 11. 下一步入口

后续不再等待额外设计裁决，默认按以下顺序进入实现设计或实现阶段：

1. 先定义状态返回结构中 `raw_io / effective_interlocks / supervision` 的正式字段清单。
2. 先落 `door` 与 `negative-limit` 两个样板场景的协议、HMI 消费和回归断言。
3. 待样板场景稳定后，再扩展到 `estop`、供胶互锁、点胶阀互锁和其他运行态边界。

若实施阶段发现协议兼容成本超预期，可以降级为“字段新增但旧字段不移除”的渐进兼容策略；不再回退到“继续把保护语义塞进 `raw_io`”的方案。
