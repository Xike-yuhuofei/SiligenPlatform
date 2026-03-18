# transport-gateway

目标定位：TCP/JSON 协议、会话、命令分发、envelope 和 facade 映射的 canonical 实现包。

## 负责什么

- `include/siligen/gateway/protocol/*`：传输层 envelope 与协议头
- `src/protocol/*`：JSON envelope 编解码
- `src/tcp/*`：TCP 会话、server、dispatcher
- `src/facades/tcp/*`：应用 use case 到传输 DTO 的 facade 映射
- `include/siligen/gateway/tcp/tcp_facade_builder.h`：面向 app 的 facade 装配入口
- `include/siligen/gateway/tcp/tcp_server_host.h`：面向 app 的 server host/wiring 入口

## 不负责什么

- 任何运动、点胶、配方、系统初始化等核心业务规则
- 容器宿主、配置加载、安全初始化、后台任务
- 应用协议 schema 的 source of truth

## 协议与依赖规则

- 方法名、fixture、兼容差异以 `D:\Projects\SiligenSuite\packages\application-contracts` 为准。
- transport-gateway 只消费契约事实，不在本包内另起一套协议定义。
- `apps/control-tcp-server` 只允许依赖本包公开入口与 `packages/runtime-host`，不得再 include `control-core/modules/control-gateway/src/*`。

## Canonical targets

- `siligen_transport_gateway`
- `siligen_transport_gateway_protocol`

兼容 alias 仍保留在 legacy shell 中：

- `siligen_control_gateway_tcp_adapter`
- `siligen_tcp_adapter`
- `siligen_control_gateway`

## 验证

```powershell
python D:\Projects\SiligenSuite\packages\application-contracts\tests\test_protocol_compatibility.py
python D:\Projects\SiligenSuite\packages\transport-gateway\tests\test_transport_gateway_compatibility.py
```
