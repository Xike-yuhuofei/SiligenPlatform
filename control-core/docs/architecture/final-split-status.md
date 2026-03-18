# 仓内模块化拆分完成状态（当前轮次）

## 结论

本轮已完成 `control-core` 仓内模块化与主实现物理收口，并进一步完成第二、第三阶段兼容壳清扫：

- `apps/control-runtime` 已承接运行时装配、系统用例、运行时监控、安全实现、配置与本地存储主实现
- `modules/control-gateway` 已承接 TCP 协议、TCP 会话/服务端、命令分发器与 TCP 门面主实现
- `modules/device-hal` 已承接 motion / dispensing / diagnostics / recipes / multicard 主实现源码
- 兼容壳已经裁到最低限度，运行时与设备侧的 canonical 路径现已稳定

当前代码库已经进入“canonical 新目录为主，legacy 仅剩极少量说明/CMake 痕迹”的稳定完成态；已不存在主实现依赖 legacy wrapper 源文件的问题。

## 已完成的主实现落点

### apps/control-runtime

已完成物理迁移：

- `apps/control-runtime/ContainerBootstrap.cpp`
- `apps/control-runtime/bootstrap/InfrastructureBindingsBuilder.cpp`
- `apps/control-runtime/container/ApplicationContainer*.cpp`
- `apps/control-runtime/runtime/events/*`
- `apps/control-runtime/runtime/scheduling/*`
- `apps/control-runtime/usecases/system/*`
- `apps/control-runtime/services/motion/HardLimitMonitorService.*`
- `apps/control-runtime/services/motion/SoftLimitMonitorService.*`
- `apps/control-runtime/runtime/configuration/*`
- `apps/control-runtime/runtime/storage/files/*`
- `apps/control-runtime/security/*`
- `apps/control-runtime/security/config/*`

运行时 canonical 路径：

- `apps/control-runtime/container/*`
- `apps/control-runtime/bootstrap/*`
- `apps/control-runtime/runtime/*`
- `apps/control-runtime/usecases/system/*`
- `apps/control-runtime/services/motion/*`
- `apps/control-runtime/security/*`

### modules/control-gateway

已完成物理迁移：

- `modules/control-gateway/src/protocol/json_protocol.cpp`
- `modules/control-gateway/src/tcp/JsonProtocol.*`
- `modules/control-gateway/src/tcp/TcpCommandDispatcher.*`
- `modules/control-gateway/src/tcp/TcpServer.*`
- `modules/control-gateway/src/tcp/TcpSession.*`
- `modules/control-gateway/src/facades/tcp/TcpSystemFacade.*`
- `modules/control-gateway/src/facades/tcp/TcpMotionFacade.*`
- `modules/control-gateway/src/facades/tcp/TcpDispensingFacade.*`
- `modules/control-gateway/src/facades/tcp/TcpRecipeFacade.*`

gateway canonical 路径：

- `modules/control-gateway/src/protocol/*`
- `modules/control-gateway/src/tcp/*`
- `modules/control-gateway/src/facades/tcp/*`
- `apps/control-tcp-server/*`

### modules/device-hal

已完成物理迁移：

- `modules/device-hal/src/drivers/multicard/*`
- `modules/device-hal/vendor/multicard/*`
- `modules/device-hal/src/adapters/motion/*`
- `modules/device-hal/src/adapters/dispensing/*`
- `modules/device-hal/src/adapters/diagnostics/*`
- `modules/device-hal/src/adapters/recipes/*`

device-hal canonical 路径：

- `modules/device-hal/src/drivers/multicard/*`
- `modules/device-hal/src/adapters/motion/*`
- `modules/device-hal/src/adapters/dispensing/*`
- `modules/device-hal/src/adapters/diagnostics/*`
- `modules/device-hal/src/adapters/recipes/*`

厂商 SDK canonical 二进制落点：

- `modules/device-hal/vendor/multicard/MultiCard.dll`
- `modules/device-hal/vendor/multicard/MultiCard.lib`
- `modules/device-hal/vendor/multicard/MultiCard.def`
- `modules/device-hal/vendor/multicard/MultiCard.exp`

当前只需把以上 `apps/*` / `modules/*` 视为主入口和主源码位置；legacy 路径不再是阅读或扩展代码时的推荐入口。

## 构建系统当前策略

为避免一次性打爆 target 依赖链，本轮采用“新 canonical target + 旧名称 alias 兼容”策略：

- `src/application/CMakeLists.txt` 现在从 `apps/control-runtime/` 与 `modules/control-gateway/` 拉取系统用例、运行时监控与 TCP 门面源码，并以 `siligen_control_application` 作为真实 target
- TCP adapter 兼容 target 现在直接从 `modules/control-gateway/src/tcp/` 拉取 TCP 适配源码
- `src/infrastructure/CMakeLists.txt` 现在从 `apps/control-runtime/` 与 `modules/device-hal/` 拉取配置、存储与硬件适配源码，并以 `siligen_control_runtime_infrastructure` / `siligen_device_hal_*` / `siligen_control_runtime_*` 作为真实 target
- 根 `CMakeLists.txt` 的 `security_module` 现在直接从 `apps/control-runtime/security/` 构建
- `MultiCard` 的链接、安装与 DLL 拷贝逻辑现在统一从 `modules/device-hal/vendor/multicard/` 取件

这意味着主实现已经迁移完成；当前保留旧 target 名称只是为了降低构建链重命名风险，默认应优先使用 canonical target 名称。

## 验证结果

第三阶段清扫与 vendor 路径收口后已再次完成以下验证：

- `cmake -S . -B build/stage4-all-modules-check -DSILIGEN_BUILD_TESTS=ON`
- `cmake --build build/stage4-all-modules-check --config Debug --target siligen_control_application siligen_device_hal_hardware siligen_control_runtime siligen_control_gateway_tcp_adapter siligen_tcp_server siligen_unit_tests`
- `ctest --test-dir build/stage4-all-modules-check -C Debug --output-on-failure -R siligen_unit_tests`
- `build/stage4-all-modules-check/bin/Debug/siligen_tcp_server.exe --help`
- `cmake -S . -B build/stage4-security-check -DBUILD_SECURITY_MODULE=ON`
- `cmake --build build/stage4-security-check --config Debug --target security_module`

验证结果：

- 单元测试通过
- TCP Server 冒烟通过
- 安全模块可单独配置并构建通过
- canonical target 可直接作为 `cmake --build --target ...` 的构建目标使用
- canonical 路径稳定，构建链与运行入口保持可用

## 当前剩余事项

当前已无“必须继续迁移的主实现目录”。剩余事项全部属于后续可选清理项：

- 继续清理仅为兼容保留的 alias 命名与历史脚本引用
- CLI 已正式退役；后续仅继续清理历史文档、脚本与注释中的兼容残留
- 继续把文档与脚本默认视角统一到 `apps/*` / `modules/*`

这些工作不再影响模块边界正确性，也不再阻塞运行时、TCP 入口或 device-hal 的主实现归属。
