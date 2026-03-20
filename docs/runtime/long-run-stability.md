# Long Run Stability

更新时间：`2026-03-19`

## 1. 本轮长稳策略

本轮没有可用真实机台环境，因此长稳只执行可复现替代项：

- mock TCP/HMI 60 秒短时 soak
- simulation regression 100 次循环
- HIL 仅做启动烟测与协议探针
- real hardware 长稳未执行

这不是整班级、跨班次、跨日的现场稳定性结论；它只能证明当前仓内可运行替代链路在短时压力下没有立即失稳。

## 2. 已执行结果

### 2.1 mock TCP/HMI 短时 soak

- elapsed_seconds=`60.91`
- dxf_cycles=`78`
- ping_failures=`0`
- status_failures=`0`
- timeouts=`0`

结论：mock 层短时稳定性通过，但仍然只是 `仅 mock 验证`。

### 2.2 simulation regression soak

- 连续 `100` 次运行 `integration/simulated-line/run_simulated_line.py`
- elapsed_seconds=`16.251`
- failures=`0`

结论：canonical fixture 在 simulation 层 100 次循环无回归漂移。

### 2.3 HIL

执行方式：

- `integration/hardware-in-loop/run_hardware_smoke.py`
- 默认拉起 `<CONTROL_APPS_BUILD_ROOT>\bin\Debug\siligen_tcp_server.exe`
- 高频端口探针 / 原始协议探针

结果：

- `run_hardware_smoke.py` 输出 `hardware smoke passed: TCP endpoint is reachable`
- 已进入稳定 TCP ready 状态
- 当前只验证到最小启动烟测，尚未执行长时 soak

结论：

- `HIL 基础烟测已通过`
- `HIL 长稳仍未执行`

### 2.4 real hardware

- `未执行`
- `无硬件环境`

## 3. 建议的下一阶段长稳门槛

当前要把“替代型稳定性”提升到“可现场签收的长稳”，至少需要补齐：

1. 在 HIL 层补一轮不少于 30 分钟的 DXF 装载、启动、暂停、恢复、报警、异常恢复。
2. 在真实机台执行至少一轮可回放 smoke，明确记录配方来源、DXF 文件、机台型号、控制卡 / IO 环境与恢复步骤。

建议执行入口（HIL 闭环/长稳）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File D:\Projects\SiligenSuite\integration\hardware-in-loop\run_hil_controlled_test.ps1 -Profile Local -HilDurationSeconds 1800 -HilPauseResumeCycles 3
```

或单独执行：

```powershell
python D:\Projects\SiligenSuite\integration\hardware-in-loop\run_hil_closed_loop.py --duration-seconds 1800 --report-dir D:\Projects\SiligenSuite\integration\reports\hil-controlled-test --dispenser-count 300 --dispenser-interval-ms 200 --dispenser-duration-ms 80 --state-wait-timeout-seconds 8
```

在以上条件未满足前，当前文档只能作为：

- `mock/simulation 稳定性已验证`
- `HIL 长稳 / real hardware 稳定性未完成`
