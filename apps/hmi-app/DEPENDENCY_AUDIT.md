# hmi-app 依赖审计

审计时间：`2026-03-18`

## 允许依赖

- `shared/contracts/application`
- `apps/runtime-gateway/transport-gateway`
- app 自身源码

## 已移除的非法依赖

1. `control-core/build/bin/**/siligen_tcp_server.exe` 的隐式目录探测
2. 历史 multicard 驱动实现目录的运行时 DLL 探测
3. 历史 vendor 二进制目录的运行时 DLL 探测
4. `apps/hmi-app -> hmi-client/scripts/run.ps1` 的反向 wrapper 依赖（对应 legacy 目录现已删除）

## 当前依赖方式

- HMI 与后端的交互只走 TCP/JSON 应用契约
- 后端自动拉起只读取显式 launcher 契约，不再读取 Core 内部实现路径
- 默认 DXF 文件候选目录只读取 `uploads/dxf`、`shared/contracts/engineering/fixtures/cases/rect_diag`，以及显式 `SILIGEN_DXF_DEFAULT_DIR`
- DXF 编辑改为外部编辑器人工流程，不再依赖 editor contracts

## 当前边界结论

- HMI 生产页预览门禁主链路只走 `apps/runtime-gateway/transport-gateway` 的 `dxf.preview.snapshot` / `dxf.preview.confirm`
- `engineering-data` 只作为 canonical preview artifact producer 与合同 owner，不再作为 HMI 运行时本地预览集成
