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
- 不再让 `apps/hmi-app` 反向依赖 `hmi-client/scripts/run.ps1`

## 运行

直接连接已启动 gateway：

```powershell
D:\Projects\SiligenSuite\apps\hmi-app\run.ps1
```

通过显式入口自动拉起 gateway：

```powershell
D:\Projects\SiligenSuite\apps\hmi-app\run.ps1 `
  -GatewayExe "D:\Deploy\Siligen\siligen_tcp_server.exe"
```

或：

```powershell
$env:SILIGEN_GATEWAY_LAUNCH_SPEC = "D:\Deploy\Siligen\gateway-launch.json"
D:\Projects\SiligenSuite\apps\hmi-app\run.ps1
```

## 兼容说明

- legacy 目录 `D:\Projects\SiligenSuite\hmi-client` 现在只保留兼容入口与文档，不再是 canonical app
- 旧环境变量 `SILIGEN_CONTROL_CORE_ROOT`、`SILIGEN_TCP_SERVER_EXE` 已由显式 gateway 契约替代
- 若仅需直连已在运行的 gateway，无需提供自动拉起参数
- DXF 编辑已改为外部编辑器人工流程，说明见 `D:\Projects\SiligenSuite\docs\runtime\external-dxf-editing.md`
