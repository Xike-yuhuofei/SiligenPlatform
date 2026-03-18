# hmi-app 兼容说明

## canonical 位置

- HMI 的 canonical 位置是 `D:\Projects\SiligenSuite\apps\hmi-app`
- `D:\Projects\SiligenSuite\hmi-client` 仅保留兼容壳与历史文档

## 启动兼容

- `hmi-client\scripts\run.ps1` 现在转发到 `apps\hmi-app\run.ps1`
- HMI 仍可自动拉起后端，但必须通过显式 launcher 契约
- 新入口优先使用 `SILIGEN_GATEWAY_LAUNCH_SPEC` 或 `-GatewayExe`

## 不再支持的隐式路径约定

- 不再隐式猜测同工作区下的 `control-core` 源码目录
- 不再隐式补齐 `src\infrastructure\drivers\multicard` 与 vendor DLL 路径
- 不再把 `apps/hmi-app` 作为 wrapper 指回 legacy 项目
