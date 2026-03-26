# Contract: 工作区模块边界与路径收敛契约

## 1. 合同标识

- Contract ID: `workspace-module-boundary-v1`
- Scope: `D:\Projects\SS-dispense-align\apps\`, `packages\`, `integration\`, `tools\`, `docs\`, `config\`, `data\`, `examples\`

## 2. 目的

把参考模板 M0-M11 的模块边界冻结到当前工作区的真实 canonical path cluster，明确 owner、允许依赖、禁止越权和 support/vendor/generated 目录的边界，供后续实施任务直接执行。

## 3. 模块映射与 owner 契约

| 模块 | 参考职责 | 当前 canonical path cluster | 当前 owner 说明 | 明确禁止 |
|---|---|---|---|---|
| M0 workflow | 串联阶段、回退、归档 | `packages\process-runtime-core\src\application` | 当前由 runtime core application 层承接顶层编排；后续若需独立包必须另行论证 | 在 app 层或 trace 层直接伪造编排状态 |
| M1 job-ingest | 任务建模、文件接入 | `apps\hmi-app`, `apps\control-cli`, `packages\application-contracts`, `packages\process-runtime-core\src\application` | 输入面负责命令与上下文接入，正式任务事实仍需通过应用编排层收口 | HMI/CLI 直接生成几何/轨迹/执行包事实 |
| M2 dxf-geometry | DXF 解析与规范几何 | `packages\engineering-data`, `packages\engineering-contracts` | 工程数据包是几何与离线预处理的 canonical owner | 在 runtime 或 HMI 中重新定义 DXF/geometry source of truth |
| M3 topology-feature | 拓扑修复与特征提取 | `packages\engineering-data` | 当前仍和 M2 同包收口，但冻结文档必须标明内部边界与未来拆分点 | 把 topology/feature 逻辑散落回 app/runtime 包 |
| M4 process-planning | 工艺规划 | `packages\process-runtime-core\planning`, `packages\process-runtime-core\src\domain` | 工艺规则和对象 owner 留在 runtime core | 在 runtime-host、gateway、device-adapters 中吸收工艺规则 |
| M5 coordinate-alignment | 坐标链、对位、补偿 | `packages\process-runtime-core\planning`, `packages\process-runtime-core\machine` | 需显式支持 `AlignmentCompensation(not_required)` | 用空引用绕过补偿阶段 |
| M6 process-path | 工艺路径 | `packages\process-runtime-core\planning` | 路径排序和工艺路径事实留在 runtime core | 在 M8 或 M9 反向重排路径 |
| M7 motion-planning | 运动规划、轨迹 | `packages\process-runtime-core\src\domain\motion` | 生产 owner 在 runtime core；`packages\simulation-engine` 仅消费验证 | runtime execution 直接改写 `MotionPlan` |
| M8 dispense-packaging | 时序、组包、离线校验 | `packages\process-runtime-core\src\domain\dispensing`, `packages\process-runtime-core\src\application\usecases\dispensing` | `DispenseTimingPlan`、`ExecutionPackage` 由 runtime core 生成 | 把 `ExecutionPackageBuilt` 视为已通过离线校验 |
| M9 runtime-execution | 预检、首件、执行、恢复 | `packages\runtime-host`, `packages\device-contracts`, `packages\device-adapters`, `apps\control-runtime`, `apps\control-tcp-server` | 宿主、设备、执行进程和 TCP 入口构成执行层 | 直接重建上游规划对象或把门禁失败写成规划失败 |
| M10 trace-diagnostics | 追溯、诊断、归档 | `packages\traceability-observability`, `integration\`, `docs\validation\` | trace owner 在 package；integration/docs 负责证据和验证呈现 | trace 反向修正历史对象事实 |
| M11 hmi-application | 命令入口、审批、展示、查询 | `apps\hmi-app`, `apps\control-cli`, `packages\application-contracts` | HMI/CLI 是应用面，不是事实源 | 直接控制设备、直接改写 planning/runtime owner 对象 |

## 4. 路径分类契约

| 路径类型 | 路径 | 规则 |
|---|---|---|
| effective canonical | `apps\`, `packages\`, `integration\`, `tools\`, `docs\`, `config\`, `data\`, `examples\` | 新默认入口、新 owner、新正式文档只能落在这些根及其明确子域 |
| support | `shared\`, `tests\`, `cmake\` | 不得扩张为新的业务 owner 根；若仍被引用，必须在冻结文档中说明用途 |
| vendor | `third_party\` | 只承载依赖，不承载业务 owner |
| generated/runtime | `build\`, `logs\`, `uploads\` | 只承载生成物、运行证据或输入 staging，不承载 source of truth |
| feature artifacts | `specs\` | 只承载规格、计划、任务和设计产物，不承载实现 owner |

## 5. 依赖方向契约

允许的默认方向:

1. `apps -> packages -> shared-kernel/device-contracts/contracts`
2. `integration -> apps/packages/root-entry-points`
3. `docs -> canonical fact sources`

明确禁止:

1. `M11 -> M9 implementation` 直控设备
2. `M9 -> M4/M7/M8 artifacts` 直接改写规划对象
3. `M10 -> 任意业务 owner` 反向修正历史事实
4. support/vendor/generated roots 成为新的默认 owner

## 6. 兼容与迁移规则

1. legacy 名称、legacy 路径、legacy fallback 只能出现在迁移映射、归档材料或显式 deprecation 证据。
2. 历史术语在正式文档中最多出现一次，并必须立刻映射到唯一规范名。
3. 所有新的 build/test/run 说明必须优先引用根级 `build.ps1`、`test.ps1`、`ci.ps1`。
4. 任何仍处于 `transitional canonical` 的路径都必须在对应轴文档中列出退出条件和验证点。
