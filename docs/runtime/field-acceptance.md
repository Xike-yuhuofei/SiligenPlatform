# Field Acceptance

更新时间：`2026-03-18`

## 1. 结论摘要

本轮针对重构后系统执行了真实可运行链路验收，覆盖：

- mock：HMI client -> TCP mock server -> mock core 行为
- simulation：工程数据导出、packages/simulation-engine fixture 回归、直接 `simulate_dxf_path.exe` 运行
- protocol compatibility：application / engineering / transport 契约兼容
- HIL：`control-core/build/bin/Debug/siligen_tcp_server.exe` 最小启动烟测与直接协议探针
- real hardware：未执行，当前无可用现场硬件环境

当前可给出的验收结论：

- `mock` 与 `simulation` 层可形成稳定替代验收，关键业务链路中的 DXF 预览、DXF 加载、执行、报警、局部暂停语义已验证。
- `HMI -> TCP -> Core` 基本链路在 `mock` 层通过；在 `HIL` 层被 `siligen_tcp_server.exe` 初始化阶段阻塞，未进入可交互 TCP 会话。
- `HIL` 当前仍依赖 legacy 兼容层：`apps/control-tcp-server/run.ps1 -DryRun` 实际解析到 `control-core/build/bin/Debug/siligen_tcp_server.exe`，`integration/hardware-in-loop/run_hardware_smoke.py` 默认目标也是同一路径。
- `real hardware` 未执行，不能宣称现场验收完成。

配套报告：

- `integration/reports/field-acceptance/root-local/workspace-validation.md`
- `integration/reports/field-acceptance/2026-03-18-field-acceptance-summary.md`
- `integration/reports/field-acceptance/2026-03-18-field-acceptance-summary.json`

## 2. 分层结果

| 层级 | 结果 | 说明 |
|---|---|---|
| mock | `partial pass` | HMI client + mock TCP server 跑通；`dxf.resume`、`recipe.list` 后端方法不存在。 |
| simulation | `pass` | engineering regression、simulation smoke/json-io、fixture regression、100 次 soak 均通过。 |
| protocol compatibility | `pass` | application / engineering / transport 契约兼容通过。 |
| HIL | `known failure` | `siligen_tcp_server.exe` 启动报 `IDiagnosticsPort 未注册`，未进入稳定 TCP 就绪态。 |
| real hardware | `not executed` | 无现场硬件环境；仅保留 mock / simulation / HIL 替代结果。 |

## 3. 关键业务链路结果

### 3.1 HMI -> TCP -> Core 基本链路

#### mock

- `TcpClient.connect()` 成功。
- `ping` 成功。
- `connect` 成功，返回 `Mock hardware connected`。
- `status` 返回 `connected=true`，X/Y 轴已使能，机台状态 `Idle`。
- `home`、`move`、`dispenser.start/pause/resume/stop`、`supply.open/close` 均成功。

#### HIL

- `apps/control-tcp-server/run.ps1 -DryRun` 通过，但仅说明 wrapper 能解析到 legacy 可执行文件。
- 直接拉起 `control-core/build/bin/Debug/siligen_tcp_server.exe` 时，进程输出：
  - `Siligen TCP Server`
  - `初始化应用容器...`
  - `错误: IDiagnosticsPort 未注册`
- 高频探针未观察到可稳定建立的 `127.0.0.1:9527` 会话。
- 结论：`HMI -> TCP -> Core` 真链路在 HIL 层未打通。

#### real hardware

- 未执行。
- 当前无“真实机台 + 真实控制卡 + 真实 IO”环境，不得写为通过。

### 3.2 DXF 加载 / 执行 / 暂停恢复

#### mock

- `DxfPipelinePreviewClient.generate_preview()` 对 `rect_diag.dxf` 返回成功：
  - entity_count=`5`
  - segment_count=`5`
  - total_length_mm=`541.4213562373095`
  - estimated_time_s=`27.071067811865476`
- `dxf.load(rect_diag.dxf)` 成功，segment_count=`120`。
- `dxf.execute(dry_run=true)` 成功；0.6 秒后进度为：
  - progress=`10.0`
  - current_segment=`12`
  - total_segments=`120`
- 已验证的“暂停”语义仅为：
  - `dispenser.pause`
  - `dxf.stop`
- “恢复”未验证通过：
  - 原始 RPC `dxf.resume` 返回 `Unknown method: dxf.resume`
  - 当前不能把“暂停恢复”写成通过

#### simulation

- `simulate_dxf_path.exe` 直接运行 `rect_diag` fixture 成功，输出：
  - total_time=`2.8289999999997995`
  - motion_distance=`541.53125096026`
  - motion_profile_count=`2829`
  - final_segment_index=`4`

#### HIL / real hardware

- 未验证通过。
- HIL 在 TCP server 初始化阶段即阻塞，无法进入 DXF 交互。

### 3.3 报警 / 异常恢复

#### mock

- `estop` 成功返回 `E-Stop`。
- 报警列表包含：
  - `A001 WARN 气压偏低`
  - `A002 INFO 待机时间过长`
  - `A003 CRIT 急停触发`
- `alarms.acknowledge(A001)` 后列表缩减。
- `alarms.clear()` 后列表清空。

#### HIL / real hardware

- 未执行。
- `process-runtime-core` 单测覆盖了 interlock / emergency stop / alarm 相关逻辑，但这属于包级自动化，不等价于现场链路验收。

### 3.4 配方相关

#### mock / HMI 本地

- `RecipeManager` 本地文件链路通过：
  - create/save/list/load/delete 全部成功
- 已证明 HMI 侧本地配方文件模型可工作。

#### TCP / Core

- 原始 RPC `recipe.list` 返回 `Unknown method: recipe.list`
- 本轮未验证 TCP/Core 配方服务闭环
- 因此“配方相关验证”只能写为：
  - `HMI 本地配方文件：通过`
  - `后端配方 RPC：未通过 / 未实现`

## 4. 长稳结果

长稳详见 `docs/runtime/long-run-stability.md`。本轮只执行了替代型 soak：

- mock TCP/HMI：`60.91s`，`78` 个 DXF 周期，`0` 次 ping/status 失败，`0` 次超时
- simulation：`run_simulated_line.py` 连续 `100` 次，`0` 次失败
- HIL：未执行长稳；基础烟测已被 `IDiagnosticsPort 未注册` 阻塞
- real hardware：未执行

## 5. 已知失败 vs 新回归 / 新发现

### 5.1 已知失败

- `control-runtime` dry-run 仍为 `BLOCKED`：仓内没有独立 `control-runtime` 可执行文件。
- `hardware-smoke` 当前为 `known_failure`：`siligen_tcp_server.exe` 启动时报 `IDiagnosticsPort 未注册`。

### 5.2 本轮新回归 / 新发现

- `integration/hardware-in-loop/run_hardware_smoke.py` 原本缺少 `main()` 入口，导致先前根级报告把 HIL 误记为通过；本轮已补入口并重跑，当前 `root-local` 报告才是可信结果。
- `dxf.resume` 在 mock TCP 后端不存在，`暂停恢复` 只能验证到“暂停 + 停止”，不能宣称恢复通过。
- `recipe.list` 在 mock TCP 后端不存在，后端配方服务未形成可验收闭环。
- `apps/hmi-app/src/hmi_client/tools/ui_qtest.py` 在 headless/offscreen 条件下两次尝试均以退出码 `1` 结束，且无 stdout/stderr；本轮未把 UI 自动化写为通过。

## 6. 当前验收结论

- 可以确认的通过范围：mock、simulation、protocol compatibility、process-runtime 包级回归。
- 可以确认的阻塞范围：HIL 真链路、后端配方 RPC、DXF 恢复语义、real hardware smoke。
- 当前不能宣称“系统现场验收完成”；更准确的结论是：
  - `mock/simulation 替代验收通过`
  - `HIL 已知失败`
  - `real hardware 未执行`
