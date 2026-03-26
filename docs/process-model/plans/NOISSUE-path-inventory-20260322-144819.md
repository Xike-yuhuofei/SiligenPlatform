# NOISSUE 路径与门禁现状清单 - Wave 2A 后基线

- 状态：Updated After Wave 2A Closeout
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-144819`

## 1. 已切到 bridge 的默认入口

1. `packages/process-runtime-core/src/application/services/dxf/DxfPbPreparationService.cpp`
   - bridge 前基线：`packages/engineering-data/scripts/dxf_to_pb.py`
   - 当前事实：`tools/engineering-data-bridge/scripts/dxf_to_pb.py`
2. `packages/process-runtime-core/src/domain/configuration/ports/IConfigurationPort.h`
   - bridge 前基线：`packages/engineering-data/scripts/path_to_trajectory.py`
   - 当前事实：`tools/engineering-data-bridge/scripts/path_to_trajectory.py`
3. `packages/process-runtime-core/src/infrastructure/adapters/planning/dxf/DXFMigrationConfig.h`
   - bridge 前基线：`packages/engineering-data`
   - 当前事实：`tools/engineering-data-bridge`
4. `integration/scenarios/run_engineering_regression.py`
   - 当前事实：`dxf_to_pb.py`、`export_simulation_input.py`、`generate_preview.py` 都通过 `tools/engineering-data-bridge/scripts/*` 调用
5. `integration/performance/collect_baselines.py`
   - 当前事实：`dxf_to_pb.py`、`export_simulation_input.py` 默认采样入口都通过 `tools/engineering-data-bridge/scripts/*` 调用

## 2. bridge 当前职责边界

1. `tools/engineering-data-bridge/scripts/dxf_to_pb.py`
2. `tools/engineering-data-bridge/scripts/path_to_trajectory.py`
3. `tools/engineering-data-bridge/scripts/export_simulation_input.py`
4. `tools/engineering-data-bridge/scripts/generate_preview.py`

统一事实：

1. 以上 4 个入口当前都只是 wrapper
2. 当前全部转调 `packages/engineering-data/scripts/*`
3. `packages/engineering-data` 仍是实现 owner，不是历史遗留 fallback

## 3. Wave 2B 仍存在的硬绑定证据

### 3.1 根级 build/test/source-root

1. `CMakeLists.txt:56`
   - `packages/runtime-host` 仍是 canonical source root
2. `CMakeLists.txt:60`
   - `packages/process-runtime-core` 仍是 canonical source root
3. `CMakeLists.txt:72`
   - workspace `third_party` 仍是上层显式依赖根
4. `CMakeLists.txt:80`
   - `packages/transport-gateway` 仍是 canonical source root
5. `CMakeLists.txt:88`
   - `config/machine/machine_config.ini` 仍被当作 canonical 默认配置
6. `tests/CMakeLists.txt:16`
   - 仍聚合 `../packages/process-runtime-core/tests`
7. `tests/CMakeLists.txt:28`
   - 仍聚合 `../packages/runtime-host/tests`

### 3.2 runtime 默认资产路径

1. `apps/control-runtime/main.cpp:21`
   - 默认配置路径仍是 `config/machine/machine_config.ini`
2. `apps/control-tcp-server/main.cpp:54`
   - 默认配置路径仍是 `config/machine/machine_config.ini`
3. `apps/control-cli/CommandLineParser.h:62`
   - 默认配置路径仍是 `config/machine/machine_config.ini`
4. `packages/runtime-host/src/container/ApplicationContainer.h:44`
   - 默认配置路径仍是 `config/machine/machine_config.ini`
5. `packages/runtime-host/src/bootstrap/InfrastructureBindingsBuilder.cpp:377`
   - 默认 recipes 根仍是 `data/recipes`
6. `packages/runtime-host/src/bootstrap/InfrastructureBindingsBuilder.cpp:384`
   - 默认 schema 根仍是 `data/schemas/recipes`
7. `packages/transport-gateway/src/tcp/TcpCommandDispatcher.cpp:626`
   - 默认配置路径仍是 `config/machine/machine_config.ini`

### 3.3 residual dependency / legacy 阻塞

1. `packages/process-runtime-core/BOUNDARIES.md:41`
   - 仍记录 `control-core/src/infrastructure/runtime/*`
2. `packages/process-runtime-core/BOUNDARIES.md:43`
   - 仍记录 `control-core/modules/device-hal/*`
3. `packages/process-runtime-core/CMakeLists.txt:89`
   - 仍显式依赖 workspace `third_party`
4. `packages/device-adapters/README.md:32`
   - 明确说明不能再通过 `control-core/third_party` 获取 include
5. `packages/device-adapters/README.md:33`
   - 明确说明 `control-core/modules/device-hal` 仍有残留待迁移
6. `apps/hmi-app/DEPENDENCY_AUDIT.md:14`
   - 仍保留 `control-core/src/infrastructure/drivers/multicard` 相关事实
7. `apps/hmi-app/DEPENDENCY_AUDIT.md:15`
   - 仍保留 `control-core/third_party/vendor/MultiCardDemoVC/bin` 相关事实

## 4. 当前判断

1. `Wave 2A` 已把离线工程链的默认入口从 owner 目录切到 bridge
2. `Wave 2B` 的真实阻塞点不在离线脚本默认值，而在 root build graph、runtime 默认资产路径和 residual dependency
3. 下一阶段若进入 `Wave 2B` 实施，必须先逐项拆解本清单中的硬绑定，而不是继续扩大 `Wave 2A` bridge 范围
