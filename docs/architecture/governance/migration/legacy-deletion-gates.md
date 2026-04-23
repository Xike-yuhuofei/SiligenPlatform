# Legacy Deletion Gates

更新时间：`2026-03-19`

## 1. 目的

本文件定义 legacy 目录的“安全删除门禁”与已删除状态记录。

## 2. 状态分级

| 分级 | 含义 |
|---|---|
| `已删除` | 目录已退出工作区默认链路，源码、脚本与契约已移除，只保留历史文档记录。 |
| `可进入删除准备` | 默认构建、运行、测试、CI 已切到 canonical；剩余工作主要是清理镜像源码、旧文档、旧脚本或残留 fallback。 |
| `尚不可切换` | 目录仍承载真实实现、真实产物或真实资产；不能按目录整体删除。 |

## 3. 当前状态总表

| 目录 | 当前状态分级 | 说明 |
|---|---|---|
| `control-core` | `尚不可切换` | config/data/HIL/alias/source-root 与 CLI fallback 已切走；`modules/shared-kernel`、`src/domain`、`src/application`、`modules/process-core`、`modules/motion-core` 已删除，但真实 C++ 库图、`third_party` 与剩余源码 owner 仍留在本目录。 |
| `hmi-client` | `已删除` | HMI 真实源码与入口已收口到 `apps/hmi-app`；历史材料已归档到 `docs/_archive/2026-03/hmi-client/`。 |
| `dxf-editor` | `已删除` | `apps/dxf-editor-app`、legacy `dxf-editor`、`packages/editor-contracts` 已按“外部编辑器人工流程”方案退出工作区默认链路。 |
| `dxf-pipeline` | `已删除` | 兼容 import/CLI 已迁入 `packages/engineering-data/src`；工作区内 `dxf-pipeline/` 目录已移除。 |
| `simulation-engine`（旧顶层目录） | `已删除` | 顶层旧目录已不存在；默认 build/test/CI 已稳定指向 `packages/simulation-engine`。 |

## 4. `control-core`

- 状态分级：`尚不可切换`

### 4.1 已完成的 fallback 清理

1. `control-runtime`、`control-tcp-server`、`control-cli` 的默认 exe 产物来源已经切到 canonical control-apps build root。
2. `config\machine\machine_config.ini` 与根级 `data\` 已成为默认配置与数据来源。
3. HIL 默认入口已经切到 `integration/hardware-in-loop/run_hardware_smoke.py` + canonical `siligen_tcp_server.exe`。
4. legacy gateway/tcp alias 曾短暂前移到 `packages/transport-gateway`；在 `2026-03-19` 审计确认消费者归零后，3 个 alias 已提前删除，`control-core/src/adapters/tcp` 与 `control-core/modules/control-gateway` 仍保持退出默认库图。
5. `apps/*`、`packages/runtime-host`、`packages/process-runtime-core`、`packages/transport-gateway` 已不再通过隐式 `CMAKE_SOURCE_DIR` 回落到 `control-core` source root。
6. `control-core/modules/shared-kernel`、`control-core/src/domain`、`control-core/src/application`、`control-core/modules/process-core`、`control-core/modules/motion-core` 已在 fresh provenance 审计和删除后根级 build 通过后物理删除。

### 4.2 当前真实职责

1. 仍是 control app 共享库图与顶层 CMake owner。
2. 仍持有 `src/shared`、`src/infrastructure`、`modules/device-hal` 等真实实现或兼容 include root。
3. 仍持有 `third_party` 与共享 PCH。
4. 仍是 control apps 当前统一 CMake source root 的 owner。

### 4.3 当前阻塞项

1. `packages/process-runtime-core` 仍依赖 `control-core/src/shared`、`control-core/src/infrastructure/adapters/planning/dxf` 与 `control-core/third_party`。
2. `packages/device-adapters`、`packages/device-contracts` 以及 control app 顶层库图仍依赖 `control-core/modules/device-hal` 或 `control-core/third_party`。
3. `control-core` 仍承担 control apps 顶层 CMake/source-root owner，整目录尚不能删除。

### 4.4 当前结论

`control-core` 现在已经没有 config/data/HIL/alias/source-root/CLI fallback 价值；HMI 运行态 residual consumer 也已归零，`modules/shared-kernel`、`src/domain`、`src/application`、`modules/process-core`、`modules/motion-core` 也已删除。剩余价值只来自 `src/shared`、`src/infrastructure`、`modules/device-hal`、`third_party` 与顶层 CMake/source-root owner。

CLI 当前状态详见 `docs/architecture/legacy-cutover-status.md`；历史 cutover 记录见 `docs/architecture/history/closeouts/control-cli-cutover.md`。

## 5. `hmi-client`

- 状态分级：`已删除`

1. HMI 真实源码、脚本与测试入口已经统一收口到 `apps/hmi-app`。
2. `hmi-client/` 目录已物理删除；历史材料归档到 `docs/_archive/2026-03/hmi-client/`。
3. `legacy-exit-check.ps1` 已将 `hmi-client` 切换为“目录必须不存在 + 路径不得回流”的归零门禁。

## 6. `dxf-editor`

- 状态分级：`已删除`

## 7. `dxf-pipeline`

- 状态分级：`已删除`

## 8. `simulation-engine`（旧顶层目录）

- 状态分级：`已删除`
