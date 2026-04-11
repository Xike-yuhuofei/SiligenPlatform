# Compatibility Shell Audit

更新时间：`2026-03-19`

## 1. 审计范围

- 输入基线：`docs/architecture/canonical-paths.md`
- 目录范围：`apps/`、`packages/`、`control-core/`，以及 `docs/_archive/2026-03/hmi-client/` 的归档口径
- 入口范围：根级 `build.ps1` / `ci.ps1` / `test.ps1`，以及当前可见的 build / import / include / script 硬编码路径

## 2. 当前结论

1. control apps 的默认 binary 入口已经切到 canonical control-apps build root。
2. `config/`、`data/`、HIL 默认入口已经摆脱 `control-core` fallback。
3. `siligen_control_gateway_tcp_adapter`、`siligen_tcp_adapter`、`siligen_control_gateway` 已在 `2026-03-19` 基于 `alias-consumer-audit.md` 提前删除；`control-core/src/adapters/tcp` 与 `control-core/modules/control-gateway` 仍未重新进入默认库图。
4. `control-core` 仍不能整目录删除，但 `modules/shared-kernel`、`src/domain`、`src/application`、`modules/process-core`、`modules/motion-core` 已在 `2026-03-19` 物理删除；剩余原因已经收敛为 `src/shared`、`src/infrastructure`、`modules/device-hal`、`third_party` 与顶层 CMake/source-root owner。
5. `hmi-client/` 已从工作区删除；HMI 历史材料只保留在根级 archive，已不再作为兼容壳存在。
6. canonical HMI 的 DXF 默认路径已收敛到 workspace canonical 目录与显式环境变量，不再探测 `control-core/uploads/dxf`。

## 3. 当前仍保留的兼容壳

当前无存活的 `hmi-client` 兼容壳；相关历史命令入口已随目录删除退出工作区。

## 4. 已移除的 control-core fallback 面

- `control-core/build/bin/**` 作为 `control-runtime`、`control-tcp-server` 默认产物来源
- `control-core/config/machine_config.ini`
- `control-core/src/infrastructure/resources/config/files/machine_config.ini`
- `control-core/data/recipes/*`
- `control-core/src/infrastructure/resources/config/files/recipes/schemas/*`
- `control-core/src/adapters/tcp` 的默认 alias 注册
- `control-core/modules/control-gateway` 的默认 alias 注册
- canonical package/app 对隐式 `CMAKE_SOURCE_DIR` source-root fallback 的依赖

## 5. 仍然阻塞 `control-core` 物理删除的事项

| 阻塞项 | 当前事实 |
|---|---|
| `packages/process-runtime-core` 仍未完全脱离 `control-core` | 仍依赖 `control-core/src/shared`、`control-core/src/infrastructure/adapters/planning/dxf` 与 `control-core/third_party` |
| `packages/device-adapters` / `packages/device-contracts` 仍依赖 `control-core` 真实源码或 `third_party` | `control-core/modules/device-hal`、`control-core/third_party` 仍不可删 |
| control apps 顶层 build root 仍锚定 `control-core` | `control-core` 继续承担顶层 CMake/source-root owner，整目录删除门禁尚未闭合 |

补充：

- CLI cutover 已完成；历史记录见 `docs/architecture/history/closeouts/control-cli-cutover.md`。
- `control-core/build/bin/**/siligen_cli.exe` 已无 live consumer，不再构成物理删除 `control-core` 的独立阻塞项。
