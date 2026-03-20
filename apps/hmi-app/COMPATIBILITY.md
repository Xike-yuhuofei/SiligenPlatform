# hmi-app 兼容说明

## canonical 位置

- HMI 的 canonical 位置是 `D:\Projects\SiligenSuite\apps\hmi-app`
- `D:\Projects\SiligenSuite\hmi-client` 已从工作区删除，历史材料归档到 `D:\Projects\SiligenSuite\docs\_archive\2026-03\hmi-client`

## 启动兼容

- 统一使用 `apps\hmi-app\run.ps1` 作为唯一 HMI 启动入口
- HMI 仍可自动拉起后端，但必须通过显式 launcher 契约
- 新入口优先使用 `SILIGEN_GATEWAY_LAUNCH_SPEC` 或 `-GatewayExe`

## 不再支持的隐式路径约定

- 不再隐式猜测同工作区下的 `control-core` 源码目录
- 不再隐式补齐 `src\infrastructure\drivers\multicard` 与 vendor DLL 路径
- 不再把 `apps/hmi-app` 作为 wrapper 指回 legacy 项目，也不再保留 legacy `hmi-client` wrapper
- 不再把 `control-core\uploads\dxf` 当作默认 DXF 候选目录；默认仅接受 workspace canonical 路径或 `SILIGEN_DXF_DEFAULT_DIR`
