# hmi-app 依赖审计

审计时间：`2026-03-18`

## 允许依赖

- `packages/application-contracts`
- `packages/transport-gateway`
- app 自身源码

## 已移除的非法依赖

1. `control-core/build/bin/**/siligen_tcp_server.exe` 的隐式目录探测
2. `control-core/src/infrastructure/drivers/multicard` 的运行时 DLL 探测
3. `control-core/third_party/vendor/MultiCardDemoVC/bin` 的运行时 DLL 探测
4. `apps/hmi-app -> hmi-client/scripts/run.ps1` 的反向 wrapper 依赖

## 当前依赖方式

- HMI 与后端的交互只走 TCP/JSON 应用契约
- 后端自动拉起只读取显式 launcher 契约，不再读取 Core 内部实现路径
- DXF 编辑改为外部编辑器人工流程，不再依赖 editor contracts

## 未决项

- `src/hmi_client/integrations/dxf_pipeline/preview_client.py` 仍保留本地 DXF 预览集成；它不再依赖 Core 内部实现，但若需要把 HMI 依赖严格收敛到 contracts/gateway 白名单，这一项还需继续外提或替换
