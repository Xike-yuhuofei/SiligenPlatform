# Field Acceptance

更新时间：`2026-03-19`

## 1. 结论摘要

本轮验收覆盖：

- mock：HMI client -> TCP mock server -> mock core 行为
- simulation：工程数据导出、`packages/simulation-engine` fixture 回归、直接 `simulate_dxf_path.exe` 运行
- protocol compatibility：application / engineering / transport 契约兼容
- HIL：`integration/hardware-in-loop/run_hardware_smoke.py` 最小启动烟测
- real hardware：未执行，当前无可用现场硬件环境

当前结论：

- `mock`、`simulation`、`protocol compatibility` 仍可形成稳定替代验收。
- `simulation` 当前仅代表受控仿真验收通过：覆盖运动路径、IO 回放与 deterministic 回归，不代表整机/工艺替代验收。
- `simulation` 的受控生产测试默认入口固定为 `integration/simulated-line/run_controlled_production_test.ps1`，并要求同时产出 `workspace-validation.*` 与 `simulated-line-summary.*`。
- `HIL` 默认入口已经切到 canonical `siligen_tcp_server.exe`，目标路径为 `<CONTROL_APPS_BUILD_ROOT>\bin\Debug\siligen_tcp_server.exe`。
- `HIL` 闭环/长稳入口固定为 `integration/hardware-in-loop/run_hil_controlled_test.ps1`（底层执行 `run_hil_closed_loop.py` 并产出结构化报告）。
- 本次本地 `hardware-smoke` 已通过，说明 canonical HIL 默认入口已具备最小启动闭环。
- `real hardware` 未执行，不能宣称现场验收完成。

## 2. 分层结果

| 层级 | 结果 | 说明 |
|---|---|---|
| mock | `partial pass` | HMI client + mock TCP server 跑通；`dxf.resume`、`recipe.list` 后端方法不存在。 |
| simulation | `pass` | engineering regression、simulation smoke/json-io、fixture regression、100 次 soak 均通过。 |
| protocol compatibility | `pass` | application / engineering / transport 契约兼容通过。 |
| HIL | `pass` | canonical `siligen_tcp_server.exe` 可被 `run_hardware_smoke.py` 拉起，TCP 端口可达。 |
| real hardware | `not executed` | 无现场硬件环境；仅保留 mock / simulation / HIL 替代结果。 |

## 3. HIL 当前事实

- `apps/control-tcp-server/run.ps1 -DryRun` 现在只验证 canonical `siligen_tcp_server.exe`。
- `integration/hardware-in-loop/run_hardware_smoke.py` 默认从 `SILIGEN_CONTROL_APPS_BUILD_ROOT` 解析 exe，并以工作区根为 `cwd`。
- `run_hardware_smoke.py` 本次输出 `hardware smoke passed: TCP endpoint is reachable`。
- 高频探针已观察到可建立的 `127.0.0.1:9527` 会话。

## 4. 关键业务链路结论

- `HMI -> TCP -> Core`：mock 通过；HIL 仍被应用初始化阻塞。
- DXF 加载 / 执行：mock 与 simulation 已验证；HIL / real hardware 未验证通过。
- 报警 / 异常恢复：mock 已验证；HIL / real hardware 未执行。
- 配方：HMI 本地文件模型可工作；后端 `recipe.list` 尚未形成验收闭环。

## 5. 当前验收结论

- 可以确认的通过范围：mock、simulation、protocol compatibility、process-runtime 包级回归。
- 可以确认的阻塞范围：后端配方 RPC、DXF 恢复语义、real hardware smoke。
- 当前更准确的结论是：
  - `mock/simulation 替代验收通过`
  - `simulation = 受控仿真验收，不等于整机/工艺验收`
  - `HIL 最小烟测通过`
  - `real hardware 未执行`
