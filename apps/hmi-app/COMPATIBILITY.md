# hmi-app 兼容说明

## canonical 位置

- HMI canonical 目录：`<repo-root>/apps/hmi-app`
- 历史 `hmi-client` 已从工作区删除，材料归档在 `docs/_archive/2026-03/hmi-client/`

## 启动兼容

- 统一使用 `apps/hmi-app/run.ps1` 作为唯一 HMI 启动入口。
- HMI 可自动拉起后端，但必须通过显式 launcher 契约。
- `apps/hmi-app/run.ps1` 会优先消费显式 `SILIGEN_GATEWAY_LAUNCH_SPEC` / `-GatewayLaunchSpec`，其次消费 `-GatewayExe` 生成临时契约。
- 官方入口在 online + autostart 场景下默认生成临时开发契约；如需消费仓内正式契约，使用 `config/gateway-launch.json` 或在显式 dry-run / 非 autostart 场景下检查该文件。
- `config/gateway-launch.json` 是验证/CI/release 必须存在的正式 launcher contract；它不再阻止工作区开发态按 canonical runtime-gateway 生成临时 dev contract。
- HMI 运行时不再直接消费 `SILIGEN_GATEWAY_EXE`；若只有 exe 路径，必须先通过 `apps/hmi-app/run.ps1 -GatewayExe <path>` 生成/注入 launch spec。
- `apps/hmi-app/scripts/run.ps1` 不再承担契约发现职责；online 模式下未注入显式契约时直接阻断。

## 不再支持的隐式路径约定

- 不再隐式猜测同工作区下的 `control-core` 源码目录。
- 不再隐式补齐历史 multicard/vender DLL 路径。
- 不再把 `apps/hmi-app` 作为 wrapper 指回 legacy 项目。
- 不再把 `config/gateway-launch.sample.json` 作为运行时默认回退契约。
- 不再把历史 DXF 目录当默认候选；默认仅接受 workspace canonical 路径或 `SILIGEN_DXF_DEFAULT_DIR`。
