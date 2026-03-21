# HMI Smoke 退出码契约

## Offline Smoke

| 脚本 | 退出码 | 含义 | 触发条件 |
| --- | ---: | --- | --- |
| `apps/hmi-app/scripts/offline-smoke.ps1` | `0` | 成功 | `ui_qtest.py --mode offline` 完成，且 GUI 骨架契约全部通过。 |
| `apps/hmi-app/scripts/offline-smoke.ps1` | `10` | GUI 断言失败 | `ui_qtest.py` 非零退出，且输出中没有超时标记。 |
| `apps/hmi-app/scripts/offline-smoke.ps1` | `11` | GUI 超时 | 输出包含 `FAIL: Timed out waiting for GUI contract completion`。 |

## Online Smoke

| 脚本 | 退出码 | 含义 | 触发条件 |
| --- | ---: | --- | --- |
| `apps/hmi-app/scripts/online-smoke.ps1` | `0` | 成功 | mock 可达，且 `ui_qtest.py --mode online --no-mock` 通过在线 GUI 契约。 |
| `apps/hmi-app/scripts/online-smoke.ps1` | `10` | GUI 断言失败 | mock ready 成功后，GUI smoke 非零退出，且输出中没有超时标记。 |
| `apps/hmi-app/scripts/online-smoke.ps1` | `11` | GUI 超时 | GUI smoke 输出包含 `FAIL: Timed out waiting for GUI contract completion`。 |
| `apps/hmi-app/scripts/online-smoke.ps1` | `20` | mock 启动失败 | 目标端口已被占用，或 mock 在进入可连接状态前提前退出。 |
| `apps/hmi-app/scripts/online-smoke.ps1` | `21` | backend ready 超时映射 | 输出中必须包含 `failure_code=SUP_BACKEND_READY_TIMEOUT` 与 `failure_stage=backend_ready`。 |

Supervisor 诊断映射（P0 最小约束）：

- 当 `online-smoke.ps1` 返回 `21` 时，输出必须包含：
  - `failure_code=SUP_BACKEND_READY_TIMEOUT`
  - `failure_stage=backend_ready`
  - 且以上映射应可在 `SUPERVISOR_DIAG` 中观测到（非纯脚本文案推断）。
- 当传入 `-ExpectFailureCode` 或 `-ExpectFailureStage` 时，`online-smoke.ps1` 必须强制启用 Supervisor 注入路径，避免走脚本级 mock 预探测分支。
- 当 `online-smoke.ps1` 返回 `20` 时，输出应包含：
  - `failure_stage=backend_starting`
- 当 `online-smoke.ps1` 返回 `0` 时，输出必须包含 `SUPERVISOR_EVENT type=stage_entered` 的最小阶段序列：
  - `backend_starting -> backend_ready -> tcp_connecting -> hardware_probing -> online_ready`
  - `SUPERVISOR_DIAG ... online_ready=true`
- 当 `online-smoke.ps1` 返回 `21` 时，输出必须包含：
  - `SUPERVISOR_EVENT type=stage_failed ... stage=backend_ready`

## 失败注入

使用 hanging mock 辅助脚本对 `21` 做黑盒验证：

```powershell
powershell -ExecutionPolicy Bypass -File 'apps/hmi-app/scripts/verify-online-ready-timeout.ps1'
```

期望结果：

- 进程退出码为 `21`
- 输出包含 `SUPERVISOR_DIAG ... failure_code=SUP_BACKEND_READY_TIMEOUT`
- 输出包含 `SUPERVISOR_DIAG ... failure_stage=backend_ready`
- 脚本返回后不存在残留的 `fake_hanging_server.py` Python 进程

如需直接调底层注入入口，也可运行：

```powershell
powershell -ExecutionPolicy Bypass -File 'apps/hmi-app/scripts/online-smoke.ps1' `
  -MockCommand 'apps/hmi-app/src/hmi_client/tools/fake_hanging_server.py' `
  -MockStartupTimeoutMs 2000
```

## Recovery 闭环验证

新增独立脚本：

```powershell
powershell -ExecutionPolicy Bypass -File 'apps/hmi-app/scripts/verify-online-recovery-loop.ps1'
```

退出码约定（与 GUI smoke 保持一致）：

| 脚本 | 退出码 | 含义 |
| --- | ---: | --- |
| `apps/hmi-app/scripts/verify-online-recovery-loop.ps1` | `0` | recovery 闭环验证成功。 |
| `apps/hmi-app/scripts/verify-online-recovery-loop.ps1` | `10` | recovery 场景断言失败（含缺失 `stage_failed@tcp_ready`、`recovery_invoked`、`stage_succeeded@online_ready` 或最终 `online_ready=true`）。 |
| `apps/hmi-app/scripts/verify-online-recovery-loop.ps1` | `11` | GUI 超时。 |

观察点要求：

- 输出必须包含 `SUPERVISOR_EVENT type=stage_failed ... stage=tcp_ready`（运行态退化观测）。
- 输出必须包含 `SUPERVISOR_EVENT type=recovery_invoked`。
- 输出必须包含 `SUPERVISOR_EVENT type=stage_succeeded ... stage=online_ready`（恢复后重新就绪）。
- 输出必须包含最终 `SUPERVISOR_DIAG ... online_ready=true`。
