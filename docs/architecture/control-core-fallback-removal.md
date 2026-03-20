# Control Core Fallback Removal

更新时间：`2026-03-19`

## 1. 目标与边界

本轮目标是切掉 `control-core` 仍承担的外围 fallback 职责，让 `control-core/` 不再因为 config/data/HIL/legacy alias/source-root 而必须保留。

本轮不做的事：

- 不改变已冻结协议边界
- 不迁移 `control-core` 内仍承载真实实现的核心源码
- 不把 CLI 未迁移命令面伪装成已完成迁移

## 2. config fallback 切换

已完成：

- `packages/runtime-host/src/bootstrap/InfrastructureBindingsBuilder.cpp`
- `packages/runtime-host/src/container/ApplicationContainer.cpp`
- `control-core/CMakeLists.txt`

当前默认配置链路：

1. 用户显式传入路径
2. `config/machine/machine_config.ini`
3. `config/machine_config.ini`

已移除的默认 fallback：

- `control-core/config/machine_config.ini`
- `control-core/src/infrastructure/resources/config/files/machine_config.ini`

保留项：

- 根级 bridge `config/machine_config.ini`
- 构建期 staging：`build/config/machine/machine_config.ini`、`build/config/machine_config.ini`

## 3. data / recipes fallback 切换

已完成：

- `packages/runtime-host/src/bootstrap/InfrastructureBindingsBuilder.cpp`

当前默认数据链路：

- recipes：`data/recipes/`
- schema：`data/schemas/recipes/`

已移除的默认 fallback：

- `control-core/data/recipes/`
- `control-core/data/recipes/schemas/`
- `control-core/src/infrastructure/resources/config/files/recipes/schemas/`

## 4. HIL 默认入口切换

当前默认入口：

- 脚本：`integration/hardware-in-loop/run_hardware_smoke.py`
- 默认 exe：`<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_tcp_server.exe`
- 进程 `cwd`：工作区根

本轮结果：

- HIL 默认入口已不再依赖 `control-core/build/bin/**`
- HIL 文档、workspace validation 与 deployment/runbook 口径已同步到 canonical TCP server
- 本次本地 `hardware-smoke` 已通过，说明 canonical HIL 默认入口可稳定拉起 TCP 端口

## 5. legacy target alias / CMake source root 清理

已完成：

- `packages/transport-gateway/CMakeLists.txt` 曾承接 3 个 legacy alias 的兼容导出；在 `2026-03-19` 审计确认消费者归零后，3 个 alias 已提前删除
- `control-core/src/CMakeLists.txt` 不再注册 `adapters/tcp`
- `control-core/modules/CMakeLists.txt` 不再注册 `control-gateway`
- `packages/runtime-host`、`packages/process-runtime-core`、`packages/transport-gateway`、三个 control app 的 CMake 已改为显式使用上层变量，不再隐式回落到 `CMAKE_SOURCE_DIR`

本轮删除的 alias：

| alias | 替代 target | 删除日期 | 删除依据 |
|---|---|---|---|
| `siligen_control_gateway_tcp_adapter` | `siligen_transport_gateway` | `2026-03-19` | `docs/architecture/alias-consumer-audit.md` |
| `siligen_tcp_adapter` | `siligen_transport_gateway` | `2026-03-19` | `docs/architecture/alias-consumer-audit.md` |
| `siligen_control_gateway` | `siligen_transport_gateway_protocol` | `2026-03-19` | `docs/architecture/alias-consumer-audit.md` |

说明：

- 审计范围内未发现任何 live CMake consumer；仓内命中只剩删除前定义点、已冻结的 `control-core` dead shell 文件与兼容测试。
- `control-core/src/adapters/tcp` / `control-core/modules/control-gateway` 目录仍在，但并未重新进入默认库图；它们将随 `control-core/` 物理删除一起退出工作区。
- `siligen_control_application`、`siligen_process_core`、`siligen_motion_core` 等 package 侧兼容 target 仍保留，但它们不再构成 `control-core` 的外围 fallback 价值。

## 6. build / test / CI 同步结果

已完成：

- `tools/scripts/legacy_exit_checks.py` 新增以下门禁：
  - control-core config fallback 禁止回流
  - control-core recipes/schema fallback 禁止回流
  - control-core gateway/tcp alias 注册禁止回流
  - canonical package/app 的 `CMAKE_SOURCE_DIR` source-root fallback 禁止回流
- `packages/transport-gateway/tests/test_transport_gateway_compatibility.py` 现在验证：
  - alias 不再由 `packages/transport-gateway` 导出
  - `control-core/src/CMakeLists.txt` 不再注册 `adapters/tcp`
  - `control-core/modules/CMakeLists.txt` 不再注册 `control-gateway`
- build/test/deployment/runbook/onboarding/config/data README 均已同步到 canonical 口径

## 7. `control-core/` 还保留的职责

1. 当前 C++ 顶层库图 owner
2. `src/domain`、`src/application`、`modules/process-core`、`modules/motion-core`、`modules/device-hal` 等真实源码 owner
3. `third_party` 与共享 PCH owner
4. 当前 control app 仍通过 `control-core` 顶层 CMake source root 参与统一构建

## 8. 已去除的 fallback 面

- config fallback：`control-core/config/*`、`control-core/src/infrastructure/resources/config/files/*`
- data fallback：`control-core/data/recipes/*`
- schema fallback：`control-core/src/infrastructure/resources/config/files/recipes/schemas/*`
- HIL 默认 exe / `cwd` 对 `control-core` 的依赖
- `control-core/src/adapters/tcp` / `control-core/modules/control-gateway` 的默认 alias 注册职责
- canonical package/app 对隐式 `CMAKE_SOURCE_DIR` source-root fallback 的依赖

## 9. 进入物理删除前还剩的门禁

1. 把 `packages/process-runtime-core`、`packages/device-adapters`、`packages/device-contracts`、`packages/shared-kernel` 对 `control-core` 真实源码和 `third_party` 的依赖迁出
2. 清理 `hmi-client/src/hmi_client/client/backend_manager.py` 对 `control-core/build/bin/**/siligen_tcp_server.exe` 与 `control-core` `cwd` 的直连
3. 保留 `docs/architecture/control-core-provenance-audit.md` 中记录的 provenance 证据，作为 `control-core/` 最终物理删除前的核对基线

CLI cutover 已完成，详见 `docs/architecture/control-cli-cutover.md`：

- 连接调试、运动、点胶、DXF、recipe 已迁入 canonical CLI
- `apps/control-cli/run.ps1` 已删除显式 `-UseLegacyFallback`
- `control-core/build/bin/**/siligen_cli.exe` 不再是默认或显式 CLI 入口
