# NOISSUE Wave 2A Path Bridge 设计

- 状态：Implemented，Pending Closeout
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 关联 blocker register：NOISSUE-blocker-register-20260322-134203.md
- 关联阶段出口：NOISSUE-wave1-exit-checklist-20260322-140814.md

## 1. 目标

`Wave 2A` 的目标不是立即搬走 `packages/engineering-data`，而是先把当前 3 个默认入口从“直接写死 `packages/engineering-data` 路径”改成“稳定 bridge 入口”，使后续物理迁移不再依赖旧路径。

## 2. Bridge 前基线与当前工作树事实

1. PB 生成脚本默认入口
   - 当前位置：`packages/process-runtime-core/src/application/services/dxf/DxfPbPreparationService.cpp`
   - bridge 前基线：`packages/engineering-data/scripts/dxf_to_pb.py`
   - 当前工作树事实：默认回退已切到 `tools/engineering-data-bridge/scripts/dxf_to_pb.py`
   - 保留兼容：仍支持 `SILIGEN_DXF_PB_COMMAND`、`SILIGEN_DXF_PROJECT_ROOT`、`SILIGEN_ENGINEERING_DATA_PYTHON` 等覆盖

2. 轨迹脚本默认入口
   - 当前位置：`packages/process-runtime-core/src/domain/configuration/ports/IConfigurationPort.h`
   - bridge 前基线：`packages/engineering-data/scripts/path_to_trajectory.py`
   - 当前工作树事实：默认字符串已切到 `tools/engineering-data-bridge/scripts/path_to_trajectory.py`
   - 保留兼容：`packages/runtime-host/src/runtime/configuration/ConfigFileAdapter.System.cpp` 继续支持从 `DXFTrajectory.script` 读取覆盖值

3. DXF pipeline root 默认入口
   - 当前位置：`packages/process-runtime-core/src/infrastructure/adapters/planning/dxf/DXFMigrationConfig.h`
   - bridge 前基线：`packages/engineering-data`
   - 当前工作树事实：默认值已切到 `tools/engineering-data-bridge`
   - 保留兼容：`DXFMigrationConfig.cpp` 继续把该字段解析为 workspace 下的 pipeline root，并用于命令行/校验逻辑

## 3. 设计决策

### 3.1 Bridge 根路径

`Wave 2A` 引入一个稳定 bridge 根：

- `tools/engineering-data-bridge/`

该根不是业务 owner，只承担“稳定路径锚点”职责。
当前状态：目录与 wrapper 已落地。

### 3.2 Bridge 内容

在 `tools/engineering-data-bridge/scripts/` 下保留稳定入口：

1. `dxf_to_pb.py`
2. `path_to_trajectory.py`
3. `export_simulation_input.py`
4. `generate_preview.py`

当前阶段这些脚本都是 wrapper：

1. 当前先转调 `packages/engineering-data/scripts/*`
2. 后续物理迁移时再转调新的 `modules/*` 或等价实现

其中前 2 个用于解除 `Wave 2A` 的 3 个默认入口阻塞，后 2 个用于工作区集成脚本与性能采样脚本的稳定路径锚点。这样调用方只改一次默认入口，后面迁移实现时不必继续改调用方。

### 3.3 三个默认值的替换目标

1. `DxfPbPreparationService` 的默认 PB 脚本回退路径
   - 从 `packages/engineering-data/scripts/dxf_to_pb.py`
   - 改为 `tools/engineering-data-bridge/scripts/dxf_to_pb.py`

2. `DxfTrajectoryConfig.script` 的默认值
   - 从 `packages/engineering-data/scripts/path_to_trajectory.py`
   - 改为 `tools/engineering-data-bridge/scripts/path_to_trajectory.py`

3. `DXFMigrationConfig.dxf_pipeline_path` 的默认值
   - 从 `packages/engineering-data`
   - 改为 `tools/engineering-data-bridge`

## 4. 明确保留的兼容规则

1. 现有环境变量与配置覆盖优先级保持不变，不在 `Wave 2A` 重新设计。
2. `DXFTrajectory.script` 的 INI 覆盖能力继续保留。
3. `SILIGEN_DXF_PB_COMMAND` 和 `SILIGEN_DXF_PROJECT_ROOT` 继续作为显式 override。
4. `packages/engineering-data` 在 bridge 接管前仍是事实 owner，不提前删除。

## 5. 实施顺序

1. 先创建 bridge 目录与 wrapper 脚本。
2. 再把 3 个默认入口统一改指向 bridge。
3. 然后补单元/集成验证，确认：
   - 无 override 时走 bridge
   - 有 override 时行为不变
4. 最后才允许讨论 `packages/engineering-data` 的物理迁移。

## 6. Wave 2A 的最小验收

1. 代码中默认入口不再直接引用 `packages/engineering-data/scripts/dxf_to_pb.py`
2. 代码中默认入口不再直接引用 `packages/engineering-data/scripts/path_to_trajectory.py`
3. `DXFMigrationConfig.dxf_pipeline_path` 默认值不再是 `packages/engineering-data`
4. 现有 `apps/packages/integration/protocol-compatibility/simulation/legacy-exit` 基线不回归
5. `packages/engineering-data` 在 bridge 切换完成前仍可作为实现 owner 存在

## 7. 当前判断

`Wave 2A` 的 bridge、3 个默认入口替换，以及 `integration/scenarios/run_engineering_regression.py` / `integration/performance/collect_baselines.py` 对 bridge 的工作区调用切换已进入当前工作树。  
当前待收口项是文档、blocker register、test-plan、path-inventory 与根级回归证据，而不是继续扩展 bridge 改动面。  
`Wave 2A` 现在仍不可开始物理迁移 `packages/engineering-data`。
