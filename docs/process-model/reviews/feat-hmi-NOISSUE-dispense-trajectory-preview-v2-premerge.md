# Findings

## P0

1. `status` 端点把“在线/就绪”硬编码成观测结果，UI 会出现明确的“假在线/假就绪”。
   - 文件：`packages/transport-gateway/src/tcp/TcpCommandDispatcher.cpp:849`
   - 风险：
     - `connected` 被固定返回 `true`
     - `machine_state` 被固定返回 `Ready`
     - `ReadInterlockSignals()` 失败时，`door` 继续投影为 `false`
   - 触发条件：
     - 实际硬件未连接
     - 后端已断开
     - 互锁端口未注入或读取失败
     - 轴处于暂停/执行/故障，但 UI 仍读取 `status`
   - 建议修复：
     - 把 `connected`、`machine_state`、互锁状态全部改成后端权威状态投影，禁止硬编码默认“正常”
     - `status` 端点需要显式表达 `disconnected` / `running` / `paused` / `estop` / `fault`
     - 互锁读取失败时不要回退成“门关闭”，而是返回显式不可判定状态或错误

## P1

1. “预览已确认”只存在于 HMI 本地内存，没有后端对应状态，违反方案里“唯一数据源”和“执行前门禁”要求。
   - 文件：
     - `apps/hmi-app/src/hmi_client/features/dispense_preview_gate/preview_gate.py:43`
     - `apps/hmi-app/src/hmi_client/ui/main_window.py:2071`
     - `packages/transport-gateway/src/tcp/TcpCommandDispatcher.cpp:1506`
     - `docs/architecture/dispense-trajectory-preview-scope-baseline-v1.md:53`
   - 风险：
     - 系统真实状态只有“plan prepared”，没有“preview confirmed”
     - `dxf.job.start` 不校验确认态，任意客户端只要拿到 `plan_id` 就可启动
     - HMI 重启、重连、多客户端并发时，确认态会丢失或被绕过
   - 触发条件：
     - 新开一个 HMI/CLI 客户端直接调用 `dxf.job.start`
     - 原 HMI 重启后恢复到同一 plan
   - 建议修复：
     - 把“预览确认”升级为后端显式状态，至少落到 plan/job 的权威状态模型
     - `job.start` 只能从 `PREVIEW_CONFIRMED` 跃迁到 `RUNNING`

2. 预览生成存在未封闭的失败路径，状态会卡死在 `GENERATING`。
   - 文件：
     - `apps/hmi-app/src/hmi_client/ui/main_window.py:260`
     - `apps/hmi-app/src/hmi_client/ui/main_window.py:1932`
     - `apps/hmi-app/src/hmi_client/ui/main_window.py:1998`
   - 风险：
     - `_generate_dxf_preview()` 先把 gate 切到 `GENERATING`
     - 当前实现仍调用未初始化的 `_dxf_preview_client`
     - 一旦抛异常，状态不会回到 `FAILED`，启动门禁会长期停留在“生成中”
   - 触发条件：
     - 任意一次点击“刷新预览”或加载 DXF 后自动生成预览
   - 建议修复：
     - 补齐唯一的预览渲染实现和初始化
     - 预览流程外围必须加异常边界，确保所有异常都收敛到 `FAILED`

3. Job 停止路径在后端确认前就清空了活动 job 指针，状态会提前跳到“空闲”。
   - 文件：
     - `packages/transport-gateway/src/tcp/TcpCommandDispatcher.cpp:1654`
     - `packages/transport-gateway/src/tcp/TcpCommandDispatcher.cpp:1887`
     - `packages/transport-gateway/src/tcp/TcpCommandDispatcher.cpp:1990`
   - 风险：
     - `dxf_task_id_` 在 `StopDxfJob()` 成功前就被清掉
     - 如果停止失败，`dxf.progress` 会直接返回 `idle`
     - UI/调用方会看到“已停止/空闲”，但真实 job 可能仍在执行
   - 触发条件：
     - 停止请求发出后，底层停止失败、超时或返回错误
   - 建议修复：
     - 只有在 `StopDxfJob()` 成功或 job 进入终态后才清理 `dxf_task_id_`
     - 停止中的中间态需要显式建模，不能直接回落为 `idle`

4. 运动状态与回零状态被合并成一个枚举，产生隐式状态和非法状态覆盖。
   - 文件：
     - `packages/process-runtime-core/src/application/usecases/motion/monitoring/MotionMonitoringUseCase.cpp:13`
     - `packages/transport-gateway/src/tcp/TcpCommandDispatcher.cpp:864`
     - `packages/transport-gateway/src/tcp/TcpCommandDispatcher.cpp:2642`
   - 风险：
     - `ApplyHomingStatusOverride()` 会把运行态 `IDLE/MOVING` 直接覆写成 `HOMED/HOMING`
     - `homed` 又从 `state == HOMED` 反推，形成循环定义
     - 结果是“已回零”和“正在运动”两个正交维度被压成一个状态源
   - 触发条件：
     - 轴已回零后再次移动、暂停、恢复或查询诊断状态
   - 建议修复：
     - 把 `motion_state` 与 `homed`/`homing_state` 分离建模
     - 网关不要再从 `state == HOMED` 反推 `homed`

5. UI 同时维护本地 job 状态和后端 job 状态，且对 `unknown` 没有收敛策略，会出现“假运行/假完成”。
   - 文件：
     - `apps/hmi-app/src/hmi_client/client/protocol.py:282`
     - `apps/hmi-app/src/hmi_client/ui/main_window.py:2187`
     - `apps/hmi-app/src/hmi_client/ui/main_window.py:2376`
   - 风险：
     - `_production_running` / `_production_paused` / `_completed_count` 先被本地写入
     - 轮询失败时协议层返回 `state=unknown`
     - UI 对 `unknown` 不做终止或回滚，上一轮“运行中/已暂停/已完成”会残留
     - 当 `current_job_id` 为空时，UI 还会自己伪造一个 `idle` 状态
   - 触发条件：
     - 轮询 `dxf.job.status` 失败
     - job id 被网关提前清掉
     - 网络抖动或后端重启
   - 建议修复：
     - 取消 UI 本地派生 job 终态，统一以后端 `job.status` 为唯一来源
     - 对 `unknown` 建立显式状态收敛，例如 `STATUS_UNCERTAIN`

## P2

1. 互锁状态自动清除，没有恢复确认或复位态，存在“假恢复”风险。
   - 文件：`packages/runtime-host/src/security/InterlockMonitor.cpp:223`
   - 风险：
     - `triggered_` 只要一次轮询判定正常就会清零
     - 没有 `LATCHED` / `RECOVERING` / `ACK_REQUIRED` 之类的恢复状态
     - 互锁抖动、瞬时读值恢复时，系统会直接观测为“已恢复”
   - 触发条件：
     - 急停/安全门输入瞬时恢复
     - 传感器抖动
   - 建议修复：
     - 把互锁设计成锁存状态机
     - 恢复至少需要显式复位事件，而不是依赖下一次正常采样

# Open Questions

1. 方案文档要求“预览确认状态（是否完成执行前确认）”作为输出，但当前是否接受它只存在于 HMI 本地而不落到后端？
2. `status` 端点是否打算继续承担 HMI 的权威机台状态输入？如果是，当前固定 `Ready/true` 不能进入合并。

# Residual Risks

1. 当前 review 只覆盖状态机与观测一致性，没有展开性能、内存和数值精度问题。
2. `mock_server` 也维护了一套与真实后端不同的状态机，若继续作为验收基线，仍可能掩盖真实链路状态问题。

# 建议下一步

1. 先补一份统一状态图：`artifact -> plan_prepared -> preview_confirmed -> job_running/paused/completed/failed/cancelled`
2. 再按该状态图收敛三个状态源：HMI、TCP Gateway、Runtime Workflow
3. 最后补状态一致性回归：停止失败、轮询失败、互锁恢复、预览异常、重连恢复
