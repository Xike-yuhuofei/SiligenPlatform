# control-tcp-server

当前 TCP 程序薄入口目录。

## Canonical 路径

- 根级 canonical app：`D:\Projects\SiligenSuite\apps\control-tcp-server`

当前默认视角：
- 本目录 `main.cpp` 只负责启动参数、容器创建和 wiring
- TCP 协议/会话/分发/facade 映射主实现位于 `D:\Projects\SiligenSuite\packages\transport-gateway`
- runtime 装配通过 `D:\Projects\SiligenSuite\packages\runtime-host` 提供
- 默认应使用 canonical target：`siligen_transport_gateway`

兼容说明：
- `modules/control-gateway` 当前仅保留协议 target 兼容壳
- `src/adapters/tcp` 当前仅保留 TCP adapter target 兼容壳
- 历史 target 名称 `siligen_control_gateway_tcp_adapter` / `siligen_tcp_adapter` 仍以 alias 形式保留

补充说明：

- 对整个工作区而言，本目录继续作为 legacy 可执行薄入口保留。
- 根级 `apps/control-tcp-server/run.ps1` 是后续统一入口 wrapper，默认不再要求调用方进入本目录运行。
