# DXF 真机真实点胶多批次回归模板 V1

更新时间：`2026-03-21`

## 1. 目的

本模板用于后续现场复测以下问题：

- 新 `artifact / plan / job` 主链在真机上是否可重复稳定执行
- 多批次执行时供料、点胶阀、运动、结束清理是否保持一致
- 单次“通过”是否只是偶发结果，还是具备重复性

本模板不替代工艺质量签收，也不替代长稳测试。

## 2. 执行信息

- 执行日期：`待填写`
- 执行人：`待填写`
- 设备：`待填写`
- TCP 服务版本：`待填写`
- DXF 文件：`待填写`
- 规划速度：`待填写`
- 目标循环数：`待填写`

## 3. 前置条件

每轮批量回归前必须同时满足：

- `connect=true`
- `machine_state=Ready`
- `X/Y enabled=true`
- `X/Y homed=true`
- `io.estop=false`
- `io.door=false`
- `alarms=[]`
- `supply_open=false`
- `valve_open=false`

若任一条件不满足，本轮不得启动。

## 4. 固定调用链

固定只使用新主链：

1. `dxf.artifact.create`
2. `dxf.plan.prepare(dry_run=false)`
3. `dxf.job.start`

不再使用旧 `dxf.execute` 兼容入口。

## 5. 单轮必采证据

每一轮至少记录以下字段：

- `artifact_id`
- `plan_id`
- `plan_fingerprint`
- `job_id`
- `segment_count`
- `estimated_time_s`
- `job.state`
- `completed_count`
- `elapsed_seconds`
- 终点 `X/Y`
- 运行中是否观测到：
  - `supply_open=true`
  - `valve_open=true`
- 结束后是否回到：
  - `supply_open=false`
  - `valve_open=false`
  - `machine_state=Ready`
- 是否出现：
  - `alarms`
  - `estop`
  - `timeout`
  - `cancelled`
  - `failed`

## 6. 多批次记录表

| 轮次 | artifact_id | plan_id | job_id | 运行结果 | 运行中供料打开 | 运行中阀打开 | 结束清理正确 | elapsed_seconds | 终点 X | 终点 Y | 异常说明 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 |
| 2 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 |
| 3 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 |
| 4 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 |
| 5 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 |

## 7. 判定规则

### 单轮判定

单轮只允许使用以下结论：

- `通过`
- `有条件通过`
- `阻塞`

单轮“通过”必须同时满足：

- `job.state=completed`
- 运行中观测到 `supply_open=true`
- 运行中观测到 `valve_open=true`
- 结束后 `supply_open=false`
- 结束后 `valve_open=false`
- 无 `alarms`
- 无 `estop`
- 无超时

### 多轮总体判定

总体只允许使用以下结论：

- `通过`
- `有条件通过`
- `阻塞`

填写规则：

- 所有轮次均“通过”，且无偶发报警、无人工干预：`通过`
- 所有轮次完成，但存在非阻断说明项：`有条件通过`
- 任一轮失败、报警、超时、需人工急停：`阻塞`

## 8. 范围声明

本模板能够支持判断：

- 真机真实点胶主链是否可重复执行
- 多批次运行时状态收敛是否一致

本模板不能直接支持判断：

- 工艺质量是否签收
- 胶量、胶形、外观是否合格
- 长时间连续生产是否稳定
