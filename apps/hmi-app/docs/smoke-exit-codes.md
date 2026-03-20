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
| `apps/hmi-app/scripts/online-smoke.ps1` | `21` | mock ready 超时 | mock 进程保持存活，但在 `-MockStartupTimeoutMs` 内始终未接受 TCP 连接。 |

## 失败注入

使用 hanging mock 辅助脚本对 `21` 做黑盒验证：

```powershell
powershell -ExecutionPolicy Bypass -File 'apps/hmi-app/scripts/verify-online-ready-timeout.ps1'
```

期望结果：

- 输出包含 `mock server readiness timeout`
- 进程退出码为 `21`
- 脚本返回后不存在残留的 `fake_hanging_server.py` Python 进程

如需直接调底层注入入口，也可运行：

```powershell
powershell -ExecutionPolicy Bypass -File 'apps/hmi-app/scripts/online-smoke.ps1' `
  -MockCommand 'apps/hmi-app/src/hmi_client/tools/fake_hanging_server.py' `
  -MockStartupTimeoutMs 2000
```
