# transport-gateway

目标定位：TCP/JSON 协议、会话、命令分发、envelope 和 facade 映射的迁移来源实现包。Wave 4 之后，正式网关宿主入口在 `apps/runtime-gateway/`，运行时 owner 与适配事实归位到 `modules/runtime-execution/` 与 `modules/runtime-execution/adapters/`。

## 负责什么

- `include/siligen/gateway/protocol/*`：传输层 envelope 与协议头
- `src/protocol/*`：JSON envelope 编解码
- `src/tcp/*`：TCP 会话、server、dispatcher
- `src/facades/tcp/*`：应用 use case 到传输 DTO 的 facade 映射
- `include/siligen/gateway/tcp/tcp_facade_builder.h`：面向 app 的 facade 装配入口
- `include/siligen/gateway/tcp/tcp_server_host.h`：面向 app 的 server host/wiring 入口

## 不负责什么

- `M9 runtime-execution` 的 formal owner 声明
- 任何运动、点胶、配方、系统初始化等核心业务规则
- 容器宿主、配置加载、安全初始化、后台任务
- 应用协议 schema 的 source of truth
- workspace config 路径解析与 fallback owner

## 协议与依赖规则

- 方法名、fixture、兼容差异以 `shared/contracts/application/` 为准。
- transport-gateway 只消费契约事实，不在本包内另起一套协议定义。
- `apps/runtime-gateway` 负责正式网关宿主装配；`apps/control-tcp-server` 仅保留兼容壳并桥接到该宿主 target。
- `apps/control-tcp-server` 只允许桥接 `apps/runtime-gateway` 暴露的宿主 target，不得再直接回退为 owner 或 include `control-core/modules/control-gateway/src/*`。
- `apps/runtime-gateway` 与 `modules/runtime-execution/runtime/host` 必须在进入 gateway 前完成配置解析；gateway 只消费 `IConfigurationPort`，不再暴露 `config_path` 合同面。

## Canonical targets

- `siligen_transport_gateway`
- `siligen_transport_gateway_protocol`

以下 3 个 legacy alias 已在 `2026-03-19` 完成消费者审计后删除，当前不得回流：

- `siligen_control_gateway_tcp_adapter`
- `siligen_tcp_adapter`
- `siligen_control_gateway`

审计证据见 `docs/architecture/alias-consumer-audit.md`。

## 验证

```powershell
python tests/contracts/test_protocol_compatibility.py
python tests/integration/protocol-compatibility/run_protocol_compatibility.py
```
