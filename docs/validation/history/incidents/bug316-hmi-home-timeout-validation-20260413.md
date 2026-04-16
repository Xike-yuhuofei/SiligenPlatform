# BUG-316 HMI Home Timeout Validation 2026-04-13

- 任务类型：`incident`
- 分支：`fix/motion/BUG-316-required-axis-homing-timeout`
- 验证日期：`2026-04-13`
- 验证目标：确认 HMI `btn-production-home-all -> home.auto` 在线链不再出现 `Request timed out (90.0s)`，并记录正式 HIL 准入情况。

## 本轮联机准入结论

- 允许执行受控在线专项。
- 风险等级：`hardware-sensitive`
- 限幅策略：
  - 先尝试 `limited-hil` quick gate（`HilDurationSeconds=60`），不做长稳放量。
  - 若正式 lane 被仓库 admission 阻断，则保留阻断证据并转入更窄的 HMI 在线专项。

## 正式 limited-hil 尝试结果

### 1. Controlled HIL 构建入口阻断

- 命令：

```powershell
& .\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 `
  -Profile Local `
  -UseTimestampedReportDir `
  -HilDurationSeconds 60 `
  -PublishLatestOnPass:$false `
  -OperatorOverrideReason "BUG-316 HMI home.auto timeout regression validation"
```

- 结果：`blocked`
- 阻断原因：
  - 根构建入口 `build.ps1 -> scripts/build/build-validation.ps1` 参数契约不一致。
  - 现象：`build-validation.ps1` 不接受 `-EnableCppCoverage`，导致 controlled HIL 在 build step 直接失败。

### 2. Controlled HIL SkipBuild 重试结果

- 命令：

```powershell
$env:SILIGEN_HIL_GATEWAY_EXE='D:\Projects\SiligenSuite\build\bin\Debug\siligen_runtime_gateway.exe'
& .\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 `
  -Profile Local `
  -SkipBuild `
  -UseTimestampedReportDir `
  -HilDurationSeconds 60 `
  -PublishLatestOnPass:$false `
  -OperatorOverrideReason "BUG-316 HMI home.auto timeout regression validation"
```

- 结果：`blocked`
- 阻断原因：
  - offline admission `Wave 8` 校验失败，非本次 BUG-316 修复点：
    - `modules/motion-planning/services` 缺失
    - `modules/motion-planning/adapters` 缺失
    - `modules/motion-planning/examples` 缺失
    - `modules/dispense-packaging/tests/integration` 缺失
    - `docs/architecture/governance/migration/bridge-exit-closeout.md` 缺失
    - workflow owner wiring 缺少 `siligen_job_ingest_contracts`
    - workflow owner wiring 缺少 `siligen_dxf_geometry_application_public`
    - workflow owner wiring 缺少 `siligen_runtime_execution_application_public`

- 结论：
  - 本轮未拿到 canonical `limited-hil PASS`。
  - 阻断点属于当前 worktree 的仓库 admission / 基础设施问题，不能作为 BUG-316 回零修复失败的证据。

## 直接 HMI 在线专项

### 1. 执行命令

```powershell
& .\apps\hmi-app\scripts\online-smoke.ps1 `
  -TimeoutMs 180000 `
  -GatewayExe D:\Projects\SiligenSuite\build\bin\Debug\siligen_runtime_gateway.exe `
  -GatewayConfig .\config\machine\machine_config.ini `
  -ExerciseRuntimeActions `
  -RuntimeActionProfile home_move `
  -ScreenshotPath .\tests\reports\online-validation\bug316-hmi-online-smoke\20260413-144543\hmi-home-move.png
```

### 2. 结果

- 结果：`failed on adjacent move_to path`
- 直接失败点：`Timed out waiting for runtime move_to changes both axes`
- 关键事实：
  - GUI online 启动链已完成：
    - `backend_starting -> backend_ready -> tcp_ready -> hardware_ready -> online_ready`
  - 本次没有在回零阶段复现 `Request timed out (90.0s)`。

### 3. 回零链关键证据

- HMI TCP 客户端日志：[`logs/hmi_tcp_client.log`](D:/Projects/wt-bug316/logs/hmi_tcp_client.log)
  - `2026-04-13 14:45:47` 发送：
    - `TX home.auto ... "params": {"timeout_ms": 120000}`
  - `2026-04-13 14:45:49` 返回：
    - `accepted=true`
    - `summary_state=completed`
    - `message="Axes ready at zero"`
    - `total_time_ms=2722`

- Gateway 日志：[`logs/tcp_server.log`](D:/Projects/wt-bug316/logs/tcp_server.log)
  - `14:45:47` 收到 `home.auto timeout_ms=120000`
  - `14:45:50` 随后收到 `move {"x":1.0,"y":1.5,"speed":8.0}`

- 截图：
  - [`hmi-home-move.png`](D:/Projects/wt-bug316/tests/reports/online-validation/bug316-hmi-online-smoke/20260413-144543/hmi-home-move.png)

## 当前结论

- BUG-316 主故障结论：`passed`
  - HMI 已按修复后的默认值发送 `home.auto timeout_ms=120000`。
  - 实机样本中 `home.auto` 在 `2.722s` 内完成，未复现旧的 `90.0s` HMI 超时。

- 邻近路径结论：`failed`
  - 同一条在线专项在后续 `move_to` 观测阶段失败。
  - 当前失败点属于回零后的相邻运动路径，不能回写为“回零修复失败”。

- 正式联机门禁结论：`blocked`
  - canonical `limited-hil` 仍被当前 worktree 的 admission / build 基础设施问题阻断。

## 建议下一步

- BUG-316 可以按“主故障在线专项已验证通过”继续收尾。
- 若要拿 formal HIL PASS，需先修复：
  - `build.ps1 -> build-validation.ps1` 参数契约问题
  - 当前 worktree 的 `Wave 8` admission 缺口
- 若要继续深挖本轮新增发现，应单开 incident 排查 `move_to` 在线路径超时。
