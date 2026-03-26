# transport-gateway 协议映射

## 1. owner 边界

- 协议方法名、fixture、兼容字段别名：`shared/contracts/application/`
- 传输运行时实现：`apps/runtime-gateway/transport-gateway/`
- 进程入口与宿主装配：`apps/runtime-gateway/`

## 2. 运行时映射落点

| 能力 | canonical 文件 |
|---|---|
| JSON envelope 编解码 | `src/protocol/json_protocol.cpp` |
| TCP 会话收发 | `src/tcp/TcpSession.cpp` |
| TCP accept loop | `src/tcp/TcpServer.cpp` |
| method 分发表 | `src/tcp/TcpCommandDispatcher.cpp` |
| 应用 facade 映射 | `src/facades/tcp/*` |
| app 侧 facade builder | `include/siligen/gateway/tcp/tcp_facade_builder.h` |
| app 侧 host wiring | `include/siligen/gateway/tcp/tcp_server_host.h` + `src/wiring/TcpServerHost.cpp` |

## 3. 当前协议事实

- 当前 success/error/event envelope 仍固定使用 `version: "1.0"`。
- `TcpCommandDispatcher.cpp` 中的 `RegisterCommand("...")` 是 transport-gateway 对契约方法集的唯一运行时注册点。
- 兼容别名与已知缺口不在本文件重复维护，统一记录于：
  - `shared/contracts/application/mappings/protocol-mapping.md`
  - `shared/contracts/application/mappings/compatibility-overrides.json`

## 4. app 薄入口规则

- `apps/runtime-gateway/main.cpp` 只允许：
  - 解析命令行
  - 调用 `BuildContainer(...)`
  - 通过 `BuildTcpFacadeBundle(*container)` 获取 facade bundle
  - 通过 `TcpServerHost` 启动 TCP server
- main 不允许再直接 new `TcpCommandDispatcher` 或绕过 `transport-gateway` 公共装配面
