# packages 子模块审计（保留 / 冻结 / 候删）

更新时间：`2026-03-18`

## 1. 审计前提

本文件采用以下前提：

- `control-core` 已被项目明确判定为**镜像冗余来源**，不再作为长期 owner 讨论。
- 因此，凡是 `packages/*` 中仍主要承担“从 `control-core` 收口边界”职责、但尚未承载真实实现的子模块，一律按“冻结”处理，而不是误判为“已迁移完成”。

## 2. 状态定义

- `保留`
  - 当前真实 owner
  - 有真实源码、契约、测试或运行入口
  - 后续新增需求应优先落在这里
- `冻结`
  - 保留但不继续扩边界
  - 典型形态：兼容说明、边界锚点、工作区包装脚本、生成代码、vendor 二进制、已写未接线代码、占位目录
- `候删`
  - 生成物、缓存、重复包装、待废弃链路残留
  - 可在确认无人依赖后优先清理

## 3. 立即候删的通用项

这些内容不属于长期架构资产，建议直接纳入清理计划：

| 路径模式 | 状态 | 证据 |
|---|---|---|
| `packages/**/__pycache__/` | `候删` | Python 运行缓存，不是源码 |
| `packages/**/**/*.egg-info/` | `候删` | 本地打包元数据，不应作为 repo 事实来源 |

当前已看到的实例：

- `packages/engineering-contracts/tests/__pycache__/`
- `packages/engineering-data/src/engineering_data.egg-info/`
- `packages/engineering-data/src/engineering_data/**/__pycache__/`
- `packages/test-kit/src/test_kit.egg-info/`
- `packages/test-kit/src/test_kit/__pycache__/`

## 4. 各 package 子模块清单

### 4.1 `packages/application-contracts`

| 子模块 | 状态 | 理由 |
|---|---|---|
| `commands/` | `保留` | 当前应用命令事实源 |
| `queries/` | `保留` | 当前应用查询事实源 |
| `models/` | `保留` | 状态、错误、事件模型事实源 |
| `fixtures/` | `保留` | 被 `tests/test_protocol_compatibility.py` 直接消费 |
| `tests/` | `保留` | 当前契约兼容性验证入口 |
| `mappings/` | `冻结` | 记录兼容别名与 override，不应演化为第二套协议 owner |
| `docs/` | `冻结` | 版本与兼容策略说明，保留但不承接运行逻辑 |

### 4.2 `packages/device-adapters`

| 子模块 | 状态 | 理由 |
|---|---|---|
| `include/` | `保留` | 当前公开设备适配头文件 |
| `src/drivers/multicard/` | `保留` | 当前真实 MultiCard wrapper 与错误映射实现 |
| `src/motion/` | `保留` | 当前真实运动设备适配实现 |
| `src/io/` | `保留` | 当前真实 IO 适配实现 |
| `src/fake/` | `保留` | 当前 fake device 实现，用于测试与无卡环境 |
| `vendor/multicard/` | `冻结` | 厂商二进制投放区，不应被当作业务源码演进 |

补充说明：

- 本包已有真实代码，应继续保留。
- 但 `CMakeLists.txt` 仍直接 include `../../control-core/modules/shared-kernel/include`，说明它是“真实 owner + 迁移欠账”状态，而不是完全收口完成。

### 4.3 `packages/device-contracts`

| 子模块 | 状态 | 理由 |
|---|---|---|
| `include/commands/` | `保留` | 设备命令契约 |
| `include/state/` | `保留` | 设备状态契约 |
| `include/capabilities/` | `保留` | 能力声明契约 |
| `include/faults/` | `保留` | 故障模型契约 |
| `include/events/` | `保留` | 设备事件契约 |
| `include/ports/` | `保留` | 设备边界端口契约 |
| `include/device_contracts.h` | `保留` | 统一聚合头 |

补充说明：

- 这是纯契约包，不应再被拆成实现包。
- 当前缺口不是“该不该留”，而是“还缺独立测试闭环”。

### 4.4 `packages/editor-contracts`

| 子模块 | 状态 | 理由 |
|---|---|---|
| `cli/` | `冻结` | 记录编辑器启动参数事实，但当前主链路代码消费者不足 |
| `schemas/` | `冻结` | 记录 notify schema 事实，但当前 HMI 代码侧未看到完整消费闭环 |
| `fixtures/` | `冻结` | 作为当前事实样例保留 |
| `docs/` | `冻结` | 版本说明与兼容说明 |

补充说明：

- 本包状态应跟随 `apps/dxf-editor-app` 一并决策。
- 如果后续确认“独立 DXF 编辑器”不是主链路能力，则本包整体会进入更高优先级的废弃评审。

### 4.5 `packages/engineering-contracts`

| 子模块 | 状态 | 理由 |
|---|---|---|
| `proto/` | `保留` | 工程契约 proto 源事实 |
| `schemas/` | `保留` | preview / simulation input schema 源事实 |
| `fixtures/` | `保留` | 工程场景样例与 golden baseline |
| `tests/` | `保留` | 契约一致性验证入口 |
| `docs/` | `冻结` | mappings/versioning 说明，辅助治理但不承接实现 |
| `tests/__pycache__/` | `候删` | 运行缓存 |

### 4.6 `packages/engineering-data`

| 子模块 | 状态 | 理由 |
|---|---|---|
| `src/engineering_data/cli/` | `保留` | canonical CLI 入口，已在 `pyproject.toml` 注册脚本 |
| `src/engineering_data/contracts/` | `保留` | 预览与 simulation input 的包内契约对象 |
| `src/engineering_data/models/` | `保留` | 几何 / 轨迹稳定模型 |
| `src/engineering_data/preview/` | `保留` | 预览 artifact 生成实现 |
| `src/engineering_data/processing/` | `保留` | DXF -> PB 预处理实现 |
| `src/engineering_data/trajectory/` | `保留` | 离线路径采样/轨迹输出实现 |
| `tests/` | `保留` | 兼容性测试入口 |
| `src/engineering_data/proto/` | `冻结` | 生成代码消费者区，真实契约源仍在 `packages/engineering-contracts/proto/` |
| `scripts/` | `冻结` | 工作区脚本包装层；内容只做 `_bootstrap + import canonical cli`，不应继续承载逻辑 |
| `docs/` | `冻结` | 边界与兼容说明 |
| `src/engineering_data.egg-info/` | `候删` | 本地打包元数据 |
| `src/engineering_data/**/__pycache__/` | `候删` | 运行缓存 |

### 4.7 `packages/process-runtime-core`

| 子模块 | 状态 | 理由 |
|---|---|---|
| `tests/` | `保留` | 当前最真实的 package 内部资产，已迁入大量 C++ 单测 |
| `process/` | `冻结` | 当前主要是边界锚点，真实实现仍依赖 legacy 映射 |
| `planning/` | `冻结` | 同上 |
| `execution/` | `冻结` | 同上 |
| `machine/` | `冻结` | 同上 |
| `safety/` | `冻结` | 同上 |
| `recipes/` | `冻结` | 同上 |
| `dispensing/` | `冻结` | 同上 |
| `diagnostics-domain/` | `冻结` | 同上 |
| `application/` | `冻结` | 同上 |
| `BOUNDARIES.md` | `冻结` | 当前是包内边界治理文件，不是实现承载 |

补充说明：

- `CMakeLists.txt` 当前仍以 `INTERFACE` target 方式把 legacy 实现收口进来。
- 这说明本包现在是**边界 owner 已建立，但源码 owner 尚未完全迁入**。
- 因此，`tests/` 是保留核心，其余子域先冻结为真实迁移落点，不要再在别处发散。

### 4.8 `packages/runtime-host`

| 子模块 | 状态 | 理由 |
|---|---|---|
| `src/ContainerBootstrap.*` | `保留` | 顶层 CMake 已直接编译 |
| `src/bootstrap/` | `保留` | 顶层 CMake 已接入 |
| `src/container/` | `保留` | 顶层 CMake 已接入 |
| `src/security/` | `保留` | 顶层 CMake 已接入 |
| `src/services/motion/` | `保留` | 顶层 CMake 已接入 |
| `src/runtime/configuration/` | `冻结` | README 认定已收口，但当前顶层 CMake 尚未编入主 target |
| `src/runtime/events/` | `冻结` | 目录内有独立 CMake，但未被顶层 `packages/runtime-host/CMakeLists.txt` 接线 |
| `src/runtime/scheduling/` | `冻结` | 同上 |
| `src/runtime/storage/files/` | `冻结` | 有源码，但未见顶层 target 接线 |

补充说明：

- 这是一个“已迁入部分真实代码，但未完全接线”的典型包。
- 后续不应再继续增殖新的 runtime 子目录，先把现有冻结区接入构建和使用链路。

### 4.9 `packages/shared-kernel`

| 子模块 | 状态 | 理由 |
|---|---|---|
| 包整体 | `冻结` | 当前只有 README，没有真实代码，仍是占位锚点 |

### 4.10 `packages/simulation-engine`

| 子模块 | 状态 | 理由 |
|---|---|---|
| `include/` | `保留` | 已承接兼容时序引擎公开头文件 |
| `src/` | `保留` | 已承接兼容时序引擎真实实现 |
| `tests/` | `保留` | 已承接 smoke/json-io 测试 |
| `examples/` | `保留` | 已承接示例程序与实现级示例资产 |
| `application/` / `runtime-bridge/` / `virtual-time/` / `virtual-devices/` / `recording/` | `冻结` | 方案 C 新框架骨架已落位，但尚未进入真实实现 |
| `docs/` / `BOUNDARIES.md` | `冻结` | 当前承接边界治理与最小闭环设计说明 |

### 4.11 `packages/test-kit`

| 子模块 | 状态 | 理由 |
|---|---|---|
| `src/test_kit/runner.py` | `保留` | 工作区验证运行核心 |
| `src/test_kit/workspace_validation.py` | `保留` | 工作区验证主入口，被根级验证流程消费 |
| `src/test_kit/__init__.py` | `保留` | 包入口 |
| `src/test_kit.egg-info/` | `候删` | 本地打包元数据 |
| `src/test_kit/__pycache__/` | `候删` | 运行缓存 |

### 4.12 `packages/traceability-observability`

| 子模块 | 状态 | 理由 |
|---|---|---|
| 包整体 | `冻结` | 当前只有 README，真实观测/存储代码仍散落在其他目录 |

### 4.13 `packages/transport-gateway`

| 子模块 | 状态 | 理由 |
|---|---|---|
| `include/` | `保留` | 当前公开头文件，面向 app 与外部链接方 |
| `src/protocol/` | `保留` | 顶层 CMake 已编译 `json_protocol.cpp` |
| `src/tcp/` | `保留` | 顶层 CMake 已编译 server/session/dispatcher |
| `src/facades/tcp/` | `保留` | 顶层 CMake 已编译 facade 映射 |
| `src/wiring/` | `保留` | 顶层 CMake 已编译 `TcpServerHost.cpp` |
| `tests/` | `保留` | 兼容性测试入口 |
| `docs/` | `冻结` | 协议映射说明，不承接实现 |

补充说明：

- 本包已有真实代码，但 `target_link_libraries()` 仍直接依赖 legacy 目标，如 `siligen_control_application`、`siligen_domain`、`storage_adapters`。
- 这说明它是“真实 owner + 迁移欠账”，不是可删包。

## 5. 总结视图

### 5.1 以“保留”为主的包

- `application-contracts`
- `device-adapters`
- `device-contracts`
- `engineering-contracts`
- `engineering-data`
- `test-kit`
- `transport-gateway`

### 5.2 以“冻结”为主的包

- `editor-contracts`
- `process-runtime-core`
- `runtime-host`
- `shared-kernel`
- `traceability-observability`

## 6. 推荐执行顺序

### P1：立即执行

1. 清理 `packages/**/__pycache__/` 与 `*.egg-info/`
2. 把 `engineering-data/scripts/` 明确标记为包装层，禁止继续写业务逻辑
3. 把 `process-runtime-core/*` README 子域视为唯一物理迁入目标，禁止再新增平行 legacy 子域

### P2：下一轮收口

1. 优先接线 `runtime-host/src/runtime/configuration`、`events`、`scheduling`、`storage/files`
2. 逐步把 `process-runtime-core` 从“README 子域 + interface target”推进到真实源码落位
3. 替换 `device-adapters`、`transport-gateway` 对 `control-core` legacy include/link 的直接依赖

### P3：产品决策后处理

1. 若确认独立 DXF 编辑器不是主链路，则将 `editor-contracts` 由“冻结”升级到“候删评审”
2. 优先把 `packages/simulation-engine/application`、`runtime-bridge`、`virtual-time`、`virtual-devices` 从设计骨架推进为真实方案 C 实现，并逐步收缩兼容时序引擎的长期职责

## 7. 一句话结论

在“`control-core` 已确定退出”的前提下，`packages/*` 当前最需要做的不是继续扩目录，而是：

- 保留真实 owner 包
- 冻结边界锚点与包装层
- 清理生成物
- 把 `process-runtime-core`、`runtime-host`、`transport-gateway` 这几类“已建包名、未完全收口”的区域尽快做实
