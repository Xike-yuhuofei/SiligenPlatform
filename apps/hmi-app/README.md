# hmi-app

`apps/hmi-app` 现在是 Siligen HMI 的 canonical 应用目录，包含真实运行源码、脚本与测试。

## 负责什么

- 承载桌面 HMI UI、用户工作流、状态展示与操作引导
- 通过 `packages/application-contracts` 定义的应用层命令/查询访问后端
- 通过显式 gateway launcher 契约拉起 `packages/transport-gateway` 对应的后端进程
- 保持 DXF 文件处理与外部编辑器人工流程的边界

## 允许依赖

- `packages/application-contracts`
- `packages/transport-gateway`
- 本 app 自身的 UI/client/workflow 代码

## 已移除的越界依赖

- 不再在 HMI 代码里硬编码 `control-core/build/bin/**/siligen_tcp_server.exe`
- 不再扫描 `control-core/src/infrastructure/drivers/**` 或 vendor DLL 目录
- 不再经由已删除的 `hmi-client/` wrapper 启动或测试 HMI

## 启动模式

`hmi-app` 现在支持两种启动语义：

- `online`：默认模式。必须完成 backend -> TCP -> hardware 启动链；任一阶段失败都视为启动失败。
- `offline`：显式离线模式。不尝试启动或连接 gateway，只拉起 UI，并将联机能力收敛为不可用状态。

## 运行

默认严格联机模式：

```powershell
D:\Projects\SiligenSuite\apps\hmi-app\run.ps1
```

显式指定联机模式并自动拉起 gateway：

```powershell
D:\Projects\SiligenSuite\apps\hmi-app\run.ps1 `
  -Mode online `
  -GatewayExe "D:\Deploy\Siligen\siligen_tcp_server.exe"
```

或使用显式 launch spec：

```powershell
$env:SILIGEN_GATEWAY_LAUNCH_SPEC = "D:\Deploy\Siligen\gateway-launch.json"
D:\Projects\SiligenSuite\apps\hmi-app\run.ps1 -Mode online
```

离线模式：

```powershell
D:\Projects\SiligenSuite\apps\hmi-app\run.ps1 -Mode offline
```

离线模式保证：

- 主窗口可稳定拉起
- 顶部状态区明确显示 `Offline`
- 联机依赖动作不会再给出误导性的“已启动/运行中/已连接”文案

离线模式不保证：

- backend/TCP/hardware 可用
- DXF 加载与执行
- 点动、回零、点胶、供料、报警确认等联机动作

## Smoke

单元测试：

```powershell
D:\Projects\SiligenSuite\apps\hmi-app\scripts\test.ps1
```

离线 GUI smoke：

```powershell
D:\Projects\SiligenSuite\apps\hmi-app\scripts\offline-smoke.ps1
```

在线 GUI smoke：

```powershell
D:\Projects\SiligenSuite\apps\hmi-app\scripts\online-smoke.ps1
```

在线 ready-timeout 失败注入验证：

```powershell
D:\Projects\SiligenSuite\apps\hmi-app\scripts\verify-online-ready-timeout.ps1
```

推荐的测试矩阵：

- `scripts\test.ps1`：跑单元测试，适合纯逻辑回归
- `scripts\offline-smoke.ps1`：验证 `offline` GUI 骨架与离线门禁
- `scripts\online-smoke.ps1`：验证 `online` + mock server 的正向 GUI 链路
- `scripts\verify-online-ready-timeout.ps1`：验证 `online-smoke.ps1` 在 mock 存活但不 ready 时稳定返回 `21`
- 根级 `test.ps1 -Suite apps` 会统一执行 `hmi-app` 的 dry-run、单元测试、offline smoke、online smoke 和 ready-timeout 失败注入验证

常见退出码：

- `offline-smoke.ps1`：`0` 成功，`10` GUI 断言失败，`11` GUI 超时
- `online-smoke.ps1`：`0` 成功，`10` GUI 断言失败，`11` GUI 超时，`20` mock 启动失败/端口冲突，`21` mock ready 超时
- 详细契约与失败注入命令见 `apps/hmi-app/docs/smoke-exit-codes.md`

## 兼容说明

- 工作区内的 legacy `hmi-client/` 目录已删除；历史材料归档到 `D:\Projects\SiligenSuite\docs\_archive\2026-03\hmi-client`
- 对外 Python 分发名和命令入口仍保留为 `hmi-client`，仅作为命名兼容，不再对应 legacy 目录
- 旧环境变量 `SILIGEN_CONTROL_CORE_ROOT`、`SILIGEN_TCP_SERVER_EXE` 已由显式 gateway 契约替代
- 若仅需直连已在运行的 gateway，无需提供自动拉起参数
- DXF 默认候选目录只保留 `uploads\dxf` 与 `packages\engineering-contracts\fixtures\cases\rect_diag`；若需覆盖，使用 `SILIGEN_DXF_DEFAULT_DIR`
- DXF 编辑已改为外部编辑器人工流程，说明见 `D:\Projects\SiligenSuite\docs\runtime\external-dxf-editing.md`
