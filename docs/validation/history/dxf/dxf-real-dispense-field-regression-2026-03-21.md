# DXF 真机真实点胶回归记录 2026-03-21

更新时间：`2026-03-21`

## 1. 执行信息

- 执行日期：`2026-03-21`
- 执行人：`Codex + Xike`
- 执行环境：`真实设备`
- TCP 服务：`siligen_tcp_server`
- 运行模式：`真实点胶`
- 调用链：`dxf.artifact.create -> dxf.plan.prepare(dry_run=false) -> dxf.job.start`

## 2. 输入与参数

- DXF 文件：
  - [b9266857-8cf4-4196-c96f-a9ad68b82bf7_1774091654564_rect_diag.dxf](/D:/Projects/SiligenSuite/uploads/dxf/b9266857-8cf4-4196-c96f-a9ad68b82bf7_1774091654564_rect_diag.dxf)
- 上传后 artifact：
  - `artifact_id=artifact-1774095315388-1`
- 规划结果：
  - `plan_id=plan-1774095315464-2`
  - `plan_fingerprint=366146cc44018813`
  - `segment_count=9`
  - `point_count=5463`
  - `total_length_mm=545.2704`
  - `estimated_time_s=54.6165`
- 执行结果：
  - `job_id=job-1774095315479-3`
  - `target_count=1`
  - `dispensing_speed_mm_s=10.0`
  - `dry_run=false`

## 3. 执行前状态

- `connect=true`
- `machine_state=Ready`
- `X/Y enabled=true`
- `X/Y homed=true`
- `X=0.0`
- `Y=0.0`
- `io.estop=false`
- `io.door=false`
- `alarms=[]`
- `supply_open=false`
- `valve_open=false`

## 4. 关键运行事实

### 4.1 真实点胶链路已生效

运行中采样明确观测到：

- 供料已打开：`supply_open=true`
- 点胶阀已打开：`valve_open=true`
- 同时轴位置持续变化，说明不是仅打开阀、不运动，也不是仅运动、不点胶

日志证据：

- [tcp_server.log:25334](/D:/Projects/SiligenSuite/logs/tcp_server.log:25334)
- [tcp_server.log:25773](/D:/Projects/SiligenSuite/logs/tcp_server.log:25773)

### 4.2 任务正常完成

- `job.state=completed`
- `completed_count=1`
- `overall_progress_percent=100`
- `elapsed_seconds=55.2364`
- 终点位置：
  - `X=100.0`
  - `Y=100.0`

### 4.3 执行结束后硬件状态正确收敛

执行结束后服务端已关闭点胶输出：

- 先停止点胶器：
  - [tcp_server.log:25780](/D:/Projects/SiligenSuite/logs/tcp_server.log:25780)
- 再关闭供胶阀：
  - [tcp_server.log:25781](/D:/Projects/SiligenSuite/logs/tcp_server.log:25781)
  - [tcp_server.log:25785](/D:/Projects/SiligenSuite/logs/tcp_server.log:25785)
- 结束状态回到：
  - `supply_open=false`
  - `valve_open=false`
  - `machine_state=Ready`
  - [tcp_server.log:25797](/D:/Projects/SiligenSuite/logs/tcp_server.log:25797)

## 5. 判定

| 项目 | 结果 | 说明 |
| --- | --- | --- |
| DXF 上传 | `通过` | `artifact.create` 成功 |
| DXF 规划 | `通过` | `plan.prepare(dry_run=false)` 成功 |
| 真机启动门禁 | `通过` | 已连接、已回零、互锁正常、无报警 |
| 真实点胶执行 | `通过` | 运行中观测到 `supply_open=true`、`valve_open=true` |
| 任务收敛 | `通过` | `job.state=completed` |
| 结束清理 | `通过` | 阀与供料已关闭，设备回到 `Ready` |

本轮结论：`通过`

## 6. 范围声明

本记录能够证明：

- 新 `artifact / plan / job` 主链已经在真机上完成一次真实点胶闭环
- 真实供料与点胶阀链路被执行，而不是 `dry_run`
- 执行完成后状态能够正确收敛

本记录不能证明：

- 胶量、胶形、落点偏差已经完成工艺签收
- 连续多批次产能、长稳和工艺一致性已经完成现场验收

## 7. 与本次主线任务的关系

以本次会话主线目标为准，当前可以确认：

- `DXF 上传 -> 规划 -> 真机真实点胶 -> 完成收敛` 已闭环
- 之前阻断主线的互锁、回零、无编码器反馈、执行完成判定、运行期进度问题，没有在本轮真实点胶回归中再次阻断主链
