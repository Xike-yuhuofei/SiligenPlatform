# Performance Baseline

更新时间：`2026-03-26`

## 1. 目的

本文档冻结单轨工作区当前可重复执行的性能基线，覆盖：

- DXF / 工程数据预处理
- simulated-line 回归
- HMI 启动序列
- TCP 基础请求响应

## 2. 采样环境

- 工作区：`D:\Projects\SiligenSuite`
- 采样时间：`2026-03-25`
- 平台：`Windows-11-10.0.26100-SP0`
- Python：`3.12.1`
- CPU：`16` 逻辑核
- 采样脚本：`tests/performance/collect_baselines.py`
- 原始报告：`tests/reports/performance/2026-03-25-baseline.json`
- 冻结快照：`tests/reports/performance/2026-03-25-baseline.md`
- 最新快照：`tests/reports/performance/latest.json`、`tests/reports/performance/latest.md`

默认采样次数：

- 预处理：`10`
- 仿真回归：`10`
- HMI 启动：`10`
- TCP 请求：`100`

## 3. 基线值（2026-03-25）

### 3.1 DXF / 工程数据预处理

测试件：`shared/contracts/engineering/fixtures/cases/rect_diag/rect_diag.dxf`

| 指标 | 中位数 | P95 | 均值 | 样本数 | 可信度 |
|---|---:|---:|---:|---:|---|
| `dxf_to_pb` | `423.517 ms` | `458.577 ms` | `427.786 ms` | `10` | `high` |
| `export_simulation_input` | `100.345 ms` | `105.588 ms` | `101.101 ms` | `10` | `high` |
| `combined_preprocess` | `523.620 ms` | `564.164 ms` | `528.887 ms` | `10` | `high` |
| `engineering_regression` | `1476.408 ms` | `1647.288 ms` | `1512.018 ms` | `10` | `medium` |

### 3.2 仿真回归

测试件：`shared/contracts/engineering/fixtures/cases/rect_diag/simulation-input.json`

| 指标 | 中位数 | P95 | 均值 | 样本数 | 可信度 |
|---|---:|---:|---:|---:|---|
| `simulated_line_regression` | `9731.106 ms` | `9987.244 ms` | `9754.022 ms` | `10` | `high` |

### 3.3 HMI 启动

| 指标 | 中位数 | P95 | 均值 | 样本数 | 可信度 |
|---|---:|---:|---:|---:|---|
| `startup_total` | `1240.963 ms` | `1250.223 ms` | `1238.825 ms` | `10` | `high` |
| `backend_start` | `515.525 ms` | `520.333 ms` | `515.657 ms` | `10` | `high` |
| `backend_wait_ready` | `713.625 ms` | `728.323 ms` | `715.131 ms` | `10` | `high` |
| `tcp_connect` | `2.036 ms` | `18.928 ms` | `6.634 ms` | `10` | `low` |
| `hardware_init` | `1.170 ms` | `2.696 ms` | `1.395 ms` | `10` | `low` |

### 3.4 TCP 基础请求响应

| 指标 | 中位数 | P95 | 均值 | 样本数 | 可信度 |
|---|---:|---:|---:|---:|---|
| `ping` | `0.061 ms` | `0.085 ms` | `0.064 ms` | `100` | `low` |
| `status` | `0.058 ms` | `0.130 ms` | `0.075 ms` | `100` | `low` |
| `connect` | `0.048 ms` | `0.078 ms` | `0.053 ms` | `100` | `low` |

## 4. 不可比较项

- 仅覆盖 `rect_diag` canonical fixture，不能直接外推到大 DXF 或复杂工艺。
- HMI 启动基线不包含 Qt 首帧渲染与真实网关初始化。
- TCP RTT 基线基于 localhost mock server，不能与跨主机或现场网络直接比较。

## 5. 重复执行

```powershell
python tests\performance\collect_baselines.py
```

## 6. 监控建议

- 首批门限建议聚焦 `combined_preprocess`、`engineering_regression`、`simulated_line_regression`、`startup_total`。
- 告警线建议以当前中位数上浮 `20%` 作为第一版阈值。
- 每次发布候选至少保留日期版 `baseline.json/.md`，不要只覆盖 `latest.*`。
