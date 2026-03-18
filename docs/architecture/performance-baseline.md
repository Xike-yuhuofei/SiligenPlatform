# Performance Baseline

更新时间：`2026-03-18`

## 1. 目的

本文档用于为重构后的 `SiligenSuite` 建立首版可重复执行的性能基线，优先覆盖当前已经具备稳定自动化入口的链路：

- DXF / 工程数据预处理
- canonical 仿真回归
- HMI 启动序列
- TCP 基础请求响应

本基线不用于一次性性能优化结论，而用于后续回归比较和门限设置。

## 2. 采样环境

- 工作区：`D:\Projects\SiligenSuite`
- 采样时间：`2026-03-18`
- 平台：`Windows-11-10.0.26100-SP0`
- Python：`3.12.1`
- CPU：`16` 逻辑核
- 原始报告：`integration/reports/performance/2026-03-18-baseline.json`
- 最新快照：`integration/reports/performance/latest.json`
- 采样脚本：`integration/performance/collect_baselines.py`

默认采样次数：

- 预处理：`10`
- 仿真回归：`10`
- HMI 启动：`10`
- TCP 请求：`100`

## 3. 基线值

### 3.1 DXF / 工程数据预处理

测试件：

- `packages/engineering-contracts/fixtures/cases/rect_diag/rect_diag.dxf`

采样方法：

1. 执行 `packages/engineering-data/scripts/dxf_to_pb.py`
2. 执行 `packages/engineering-data/scripts/export_simulation_input.py`
3. 执行 `integration/scenarios/run_engineering_regression.py`
4. 每轮使用临时输出目录，避免历史文件影响

基线值：

| 指标 | 中位数 | P95 | 均值 | 样本数 | 可信度 |
|---|---:|---:|---:|---:|---|
| `dxf_to_pb` | `368.316 ms` | `414.237 ms` | `376.867 ms` | `10` | `medium` |
| `export_simulation_input` | `87.356 ms` | `89.149 ms` | `87.438 ms` | `10` | `high` |
| `combined_preprocess` | `455.759 ms` | `503.079 ms` | `464.305 ms` | `10` | `medium` |
| `engineering_regression` | `541.453 ms` | `596.550 ms` | `552.575 ms` | `10` | `high` |

说明：

- 当前“规划/工程链路”以 `rect_diag.dxf -> PathBundle -> simulation-input -> engineering_regression` 作为首版基线。
- `dxf_to_pb` 和整链路存在轻微波动，主要来自 Python 进程启动和 DXF 解析开销，因此标记为 `medium`。

### 3.2 仿真回归

测试件：

- `packages/engineering-contracts/fixtures/cases/rect_diag/simulation-input.json`

采样方法：

1. 执行 `integration/simulated-line/run_simulated_line.py`
2. 直接执行 `packages/simulation-engine/build/Debug/simulate_dxf_path.exe`
3. 每轮使用临时输出文件，避免结果缓存

基线值：

| 指标 | 中位数 | P95 | 均值 | 样本数 | 可信度 |
|---|---:|---:|---:|---:|---|
| `simulated_line_regression` | `166.878 ms` | `184.664 ms` | `168.789 ms` | `10` | `medium` |
| `simulate_dxf_path_raw` | `106.524 ms` | `115.519 ms` | `106.978 ms` | `10` | `high` |

说明：

- `simulated_line_regression` 包含 Python 包装层与基线比对，因此比裸 `simulate_dxf_path.exe` 更适合作为工作区回归指标。

### 3.3 HMI 启动

采样方法：

1. 复用 `apps/hmi-app/src/hmi_client/client/startup_sequence.py` 的三阶段逻辑
2. 用 `apps/hmi-app/src/hmi_client/tools/mock_server.py` 作为 backend
3. 测量 `backend.start -> wait_ready -> TcpClient.connect -> connect_hardware`
4. 每轮使用新的 localhost 端口，避免端口复用影响

基线值：

| 指标 | 中位数 | P95 | 均值 | 样本数 | 可信度 |
|---|---:|---:|---:|---:|---|
| `startup_total` | `1241.958 ms` | `1271.819 ms` | `1245.766 ms` | `10` | `high` |
| `backend_start` | `520.854 ms` | `539.006 ms` | `522.861 ms` | `10` | `high` |
| `backend_wait_ready` | `726.065 ms` | `733.927 ms` | `720.239 ms` | `10` | `high` |
| `tcp_connect` | `0.824 ms` | `1.497 ms` | `0.899 ms` | `10` | `low` |
| `hardware_init` | `0.955 ms` | `5.014 ms` | `1.763 ms` | `10` | `low` |

说明：

- 当前最稳的启动基线是 `startup_total`，而不是两个亚毫秒子阶段。
- `tcp_connect` 和 `hardware_init` 量级过小，容易被 Windows 调度和计时器粒度放大，因此只适合观察数量级，不适合设严格阈值。

### 3.4 TCP 基础请求响应

采样方法：

1. 启动 localhost mock server
2. 建立单一 TCP 连接
3. 分别对 `ping`、`status`、`connect` 各采样 `100` 次
4. 只记录应用层请求往返时间，不含建连阶段

基线值：

| 指标 | 中位数 | P95 | 均值 | 样本数 | 可信度 |
|---|---:|---:|---:|---:|---|
| `ping` | `0.060 ms` | `0.100 ms` | `0.065 ms` | `100` | `low` |
| `status` | `0.069 ms` | `0.134 ms` | `0.080 ms` | `100` | `low` |
| `connect` | `0.056 ms` | `0.118 ms` | `0.068 ms` | `100` | `low` |

说明：

- 这些结果证明本地 mock 协议栈本身没有明显性能瓶颈。
- 由于它们处于亚毫秒量级，当前更适合作为“异常放大检测”而不是“精细优化指标”。

## 4. 不可比较项

- 本基线只覆盖 canonical `rect_diag` fixture，不能直接外推到大 DXF、多层工艺或高实体数工程。
- HMI 启动基线是 mock backend 的启动序列，不包含 Qt 首帧渲染、窗口显示、GPU/字体初始化。
- TCP RTT 基线基于 localhost mock server，不能与真实控制卡、跨主机网络或现场 HIL 对比。
- 同一天内的环境也可能变化；若后续 build、驱动或依赖有更新，必须重新采样后再比较。

## 5. 手工/补充测量缺口

当前尚未自动采集的性能项：

- 真实 HMI 首帧时间
- 真实 HIL / 现场 TCP RTT
- 大型工程文件预处理耗时分布

建议手工测量方式：

1. HMI 首帧：使用 `apps/hmi-app/src/hmi_client/tools/ui_qtest.py`，记录进程启动到主窗口 ready 的时间戳。
2. 真实 HIL RTT：在 HIL 环境对 `ping` / `status` 做同样的 `100` 次采样，并单独存档，不与 mock 混合。
3. 大工程预处理：挑选代表性 DXF 资产，复用 `collect_baselines.py` 的预处理逻辑追加 fixture 维度。

## 6. 重复执行方式

```powershell
python integration\performance\collect_baselines.py
```

如需降低亚毫秒请求的噪声，可提高 TCP 采样数：

```powershell
python integration\performance\collect_baselines.py --tcp-iterations 300
```

## 7. 后续监控建议

- 把 `combined_preprocess`、`engineering_regression`、`simulated_line_regression`、`startup_total` 作为首批回归门限指标。
- 首版告警线可用“当前中位数上浮 `20%`”。
- 对亚毫秒 TCP 指标只做异常放大告警，例如中位数超过当前 `3x` 再报警。
- 每次发布候选构建至少保留一份日期版 JSON/Markdown 报告，避免只覆盖 `latest.*`。
