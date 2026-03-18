# control-gateway

legacy 兼容壳目录。

## 当前职责

- 保留历史 target 名称 `siligen_control_gateway`
- 把旧 CMake 入口转发到 canonical target `siligen_transport_gateway_protocol`
- 在旧路径上给阅读者留下迁移说明

## 不再承载

- TCP 协议实现
- 会话/分发代码
- TCP facade 映射
- app 启动链路

## canonical 路径

- `D:\Projects\SiligenSuite\packages\transport-gateway`

## 兼容说明

- `control-core/src/adapters/tcp` 保留 adapter alias 壳
- `control-core/apps/control-tcp-server` 保留 thin 启动入口
