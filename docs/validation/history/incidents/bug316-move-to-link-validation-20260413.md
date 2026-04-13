# BUG-316 Move-To Link Validation 2026-04-13

- 任务类型：`incident`
- 分支：`fix/motion/BUG-316-required-axis-homing-timeout`
- 验证日期：`2026-04-13`
- 验证目标：修复 HMI `move_to` 在线链的部分执行 / 整体报错问题，并保留实机证据。

## 根因结论

- gateway `HandleMove` 在完成 homed / interlock / readiness / limit 预检查后，仍以两个单轴 manual move 顺序下发：
  - 先发 X：`ExecutePointToPointMotion(cmdX)`
  - 再发 Y：`ExecutePointToPointMotion(cmdY)`
- `ManualMotionControlUseCase::ExecutePointToPointMotion()` 每次单轴下发前都会再次执行 `readiness_service_->Evaluate()`。
- 结果是：
  - X 轴启动后，系统进入“运动中”
  - Y 轴紧接着再次检查 readiness 时被判为 `motion_not_ready`
  - TCP `move` 最终返回 error，但 X 已部分执行，形成“部分执行 + 整体报错”。

## 修复内容

- HMI：
  - `protocol.move_to()` 改为透传 `error_code` / `message`
  - `_on_move_to()` 复用统一运动前置检查，并显示真实失败原因
- gateway：
  - `TcpCommandDispatcher::HandleMove()` 改为通过 `TcpMotionFacade::MoveToPosition()` 走底层同步双轴位置控制
  - 不再在 gateway 内顺序调用两次单轴 `ExecutePointToPointMotion()`

## 离线验证

- 命令：

```powershell
python -m pytest .\apps\hmi-app\tests\unit\test_protocol_preview_gate_contract.py .\apps\hmi-app\tests\unit\test_main_window.py -q
python .\apps\runtime-gateway\transport-gateway\tests\test_transport_gateway_compatibility.py
```

- 结果：
  - `132 passed in 2.81s`
  - `transport-gateway compatibility checks passed`

## 构建说明

- 根包装脚本 `build.ps1` 仍受已知参数契约问题阻断：`build-validation.ps1` 不接受 `-EnableCppCoverage`
- canonical build gate 仍受 workspace layout admission 阻断
- 为验证本次修复，使用独立短路径构建目录手工编译：

```powershell
cmake -S . -B D:\b\w316 -DSILIGEN_BUILD_TESTS=OFF -DSILIGEN_USE_PCH=OFF -DSILIGEN_PARALLEL_COMPILE=ON
cmake --build D:\b\w316 --config Debug --target siligen_runtime_gateway --parallel
```

- 产物：
  - `D:\b\w316\bin\Debug\siligen_runtime_gateway.exe`

## 在线验证

### 1. 执行命令

```powershell
& .\apps\hmi-app\scripts\online-smoke.ps1 `
  -TimeoutMs 180000 `
  -GatewayExe D:\b\w316\bin\Debug\siligen_runtime_gateway.exe `
  -GatewayConfig .\config\machine\machine_config.ini `
  -ExerciseRuntimeActions `
  -RuntimeActionProfile home_move `
  -ScreenshotPath .\tests\reports\online-validation\bug316-hmi-online-smoke\20260413-movefix-rerun2\hmi-home-move.png
```

### 2. 结果

- 结果：`passed`
- GUI 链：`backend_starting -> backend_ready -> tcp_ready -> hardware_ready -> online_ready`
- `home.auto` 正常完成后，`move` 被接受，且状态回读显示 X/Y 两轴同时进入运动

## 关键证据

- HMI TCP 客户端日志：[`logs/hmi_tcp_client.log`](D:/Projects/wt-bug316/logs/hmi_tcp_client.log)
  - `2026-04-13 18:32:16`:
    - `TX home.auto ... "params": {"timeout_ms": 120000}`
  - `2026-04-13 18:32:18`:
    - `RX home.auto ... summary_state=completed, message="Axes ready at zero"`

- Gateway 日志：[`logs/tcp_server.log`](D:/Projects/wt-bug316/logs/tcp_server.log)
  - `18:32:18.560`:
    - `TCP RX ... "method": "move", "params": {"x": 1.0, "y": 1.5, "speed": 8.0}`
  - `18:32:18.602`:
    - `TCP TX ... {"result":{"moving":true}}`
  - `18:32:18.645`:
    - 状态回读：`X position≈0.10 velocity=8.0`, `Y position≈0.15 velocity=8.0`
  - `18:32:18.743`:
    - 状态回读：`X position≈0.885 velocity=8.0`, `Y position≈0.935 velocity=8.0`

- 截图：
  - [`hmi-home-move.png`](D:/Projects/wt-bug316/tests/reports/online-validation/bug316-hmi-online-smoke/20260413-movefix-rerun2/hmi-home-move.png)

## 当前结论

- `move_to` 主故障已修复：
  - 不再出现“X 已动、Y 未动、TCP 返回 motion_not_ready”的部分执行链路
  - 实机 smoke 已验证 `move` 可被接受，且 X/Y 两轴同时进入运动
- HMI 错误透传也已补齐：
  - 若后端后续拒绝，UI 会显示真实错误码与错误消息
