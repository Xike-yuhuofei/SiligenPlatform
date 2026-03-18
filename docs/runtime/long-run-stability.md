# Long Run Stability

更新时间：`2026-03-18`

## 1. 本轮长稳策略

本轮没有可用真实机台环境，因此长稳只执行可复现替代项：

- mock TCP/HMI 60 秒短时 soak
- simulation regression 100 次循环
- HIL 仅做启动烟测与协议探针
- real hardware 长稳未执行

这不是整班级、跨班次、跨日的现场稳定性结论；它只能证明当前仓内可运行替代链路在短时压力下没有立即失稳。

## 2. 已执行结果

### 2.1 mock TCP/HMI 短时 soak

执行方式：

- 启动内嵌 `MockTcpServer`
- `TcpClient` 建立连接并完成 `connect_hardware`
- 预先 `dxf.load(rect_diag.dxf)`
- 60 秒内循环执行：
  - `ping`
  - `status`
  - 周期性 `dxf.execute(dry_run=true)` + `dxf.stop`

结果：

- elapsed_seconds=`60.91`
- dxf_cycles=`78`
- ping_failures=`0`
- status_failures=`0`
- timeouts=`0`

结论：

- mock 层短时稳定性通过
- 但这仍然是 `仅 mock 验证`

### 2.2 simulation regression soak

执行方式：

- 连续 `100` 次运行 `integration/simulated-line/run_simulated_line.py`

结果：

- elapsed_seconds=`16.251`
- failures=`0`

结论：

- canonical fixture 在 simulation 层 100 次循环无回归漂移
- 该结论只覆盖 deterministic simulation，不覆盖硬件、驱动、现场 IO

### 2.3 HIL

执行方式：

- `integration/hardware-in-loop/run_hardware_smoke.py`
- 直接拉起 `control-core/build/bin/Debug/siligen_tcp_server.exe`
- 高频端口探针 / 原始协议探针

结果：

- server 输出 `错误: IDiagnosticsPort 未注册`
- 进程退出码=`1`
- root HIL smoke 归类为 `known_failure`
- 未进入稳定 TCP ready 状态，因此未执行后续 soak

结论：

- `HIL 长稳未执行`
- 阻塞原因为基础启动即失败，而不是 soak 中途掉线

### 2.4 real hardware

结果：

- `未执行`
- `无硬件环境`

## 3. 建议的下一阶段长稳门槛

当前要把“替代型稳定性”提升到“可现场签收的长稳”，至少需要补齐：

1. 修复 `IDiagnosticsPort` 装配，使 HIL TCP server 能稳定启动。
2. 在 HIL 层补一轮不少于 30 分钟的：
   - DXF 装载
   - 启动
   - 暂停
   - 恢复
   - 报警
   - 异常恢复
3. 在真实机台执行至少一轮可回放 smoke，明确记录：
   - 配方来源
   - DXF 文件
   - 机台型号
   - 控制卡 / IO 环境
   - 失败点与恢复步骤

在以上条件未满足前，当前文档只能作为：

- `mock/simulation 稳定性已验证`
- `HIL / real hardware 稳定性未完成`
