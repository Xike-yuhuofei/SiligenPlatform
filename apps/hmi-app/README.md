# hmi-app

`apps/hmi-app/` 是 `M11 hmi-application` 的桌面宿主入口。

## 宿主职责

- 承载桌面进程、窗口生命周期与 UI 资源装配。
- 通过 formal contracts 转发任务接入、审批与查询请求到 `modules/hmi-application/`。
- 通过显式 launcher contract 管理 `runtime-gateway` 启动与连接装配。

## 明确禁止

- 承载 `M11` 终态业务 owner 事实或 owner 专属契约定义。
- 直接生成或改写 `M0/M4-M9` 核心对象事实。
- 直接越权调用设备实现。
- 把 DXF 外部编辑流程重新收回为内建应用能力。

## Owner 对齐

- `M11` owner 入口：`modules/hmi-application/CMakeLists.txt`、`modules/hmi-application/contracts/README.md`。
- `apps/hmi-app/` 只保留宿主与装配，不替代模块 owner。
- 跨模块稳定应用契约统一维护在 `shared/contracts/application/`。

## launcher 与 DXF 规则

- gateway 启动必须通过显式 launcher contract 配置。
- 官方入口是 `apps/hmi-app/run.ps1`；该入口会优先消费显式 `gateway-launch.json` / `-GatewayExe`，在工作区 online 开发态默认生成临时 launch contract 后再启动 HMI。
- `apps/hmi-app/scripts/run.ps1` 只作为内部 runner；online 模式下不再允许在无显式契约前提下直接启动。
- `config/gateway-launch.sample.json` 仅用于示例/部署模板，不再作为运行时默认回退来源。
- `config/gateway-launch.json` 是正式 launcher contract；用于验证/CI/release 和部署语义固化。
- HMI 运行时只消费 launch spec；`-GatewayExe` 只作为 `run.ps1` 生成临时 spec 的输入，不再作为运行时直启旁路。
- `test.ps1`、`scripts/validation/run-local-validation-gate.ps1`、`ci.ps1`、`release-check.ps1` 现在都会前置要求正式 `config/gateway-launch.json` 存在；缺失时直接失败，不再回退到开发态临时契约。
- DXF 编辑保持为外部编辑器人工流程；HMI 只负责接入文件与展示状态，不承接编辑 owner。
- 任何 online/offline 模式都不能改写 owner 边界。
