# NOISSUE Blocker Register - 整仓目录重构前置准备

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 关联范围文档：NOISSUE-scope-20260322-130211.md
- 关联架构文档：NOISSUE-arch-plan-20260322-130211.md
- 关联映射文档：NOISSUE-dir-map-20260322-130211.md
- 关联路径清单：NOISSUE-path-inventory-20260322-130211.md

## 1. 目的

本登记表只记录会阻断目录重构推进的阻塞项，不记录一般性优化项。

判定口径：

1. `Wave 1` 只允许做 `shared/*` 边界冻结与治理落地。
2. `Wave 2A` 关注 `packages/engineering-data` 的离线工程链切分。
3. `Wave 2B` 关注 `packages/process-runtime-core`、`packages/runtime-host`、`packages/device-adapters`、`packages/traceability-observability` 的运行时链切分。
4. 任何项若会导致默认入口、默认路径、build/test/CI 证据链失真，即视为物理迁移阻断项。

## 2. Wave 1 Blockers

| 阻塞项 | Owner | 解除条件 | 影响范围 | 是否阻断物理迁移 |
|---|---|---|---|---|
| `shared-kernel` 当前仍混装 kernel、logging、testing、trace/context 与部分业务倾向类型 | 架构治理 owner | 完成逻辑拆账，明确 `shared/kernel`、`shared/logging`、`shared/testing` 的边界与非承载内容 | `shared-kernel`、`packages/traceability-observability`、`packages/test-kit`、`packages/process-runtime-core`、`packages/runtime-host` | 是 |
| `application-contracts` 与 `engineering-contracts` 仍混有 fixtures/tests | 契约 owner + 测试 owner | fixtures/tests 从 future `shared/contracts/*` 边界剥离，分别落到 `shared/testing` 或 `integration` / `data` | `packages/application-contracts`、`packages/engineering-contracts`、`packages/test-kit`、`integration/*` | 是 |
| `shared/logging` 尚无独立稳定边界 | 架构治理 owner | 明确抽象、实现、消费者三层归属，并冻结不纳入追踪/查询业务模型 | `packages/shared-kernel`、`packages/traceability-observability`、`packages/runtime-host`、`packages/process-runtime-core`、`packages/transport-gateway` | 是 |
| `shared/testing` 与 integration 场景脚本边界未完全分离 | 测试 owner | 明确 `shared/testing` 只承接 runner/report model/known-failure 分类，场景 gate 留在 `integration/` | `packages/test-kit`、`integration/hardware-in-loop`、`integration/simulated-line`、`build/test/CI` | 是 |

## 3. Wave 2A Blockers

| 阻塞项 | Owner | 解除条件 | 影响范围 | 是否阻断物理迁移 |
|---|---|---|---|---|
| `packages/engineering-data` 仍绑定 `dxf_to_pb.py` | 离线工程链 owner | 提供 path bridge 或直接替换默认入口 | `packages/engineering-data`、`packages/process-runtime-core`、`integration/scenarios` | 是 |
| `packages/engineering-data` 仍绑定 `path_to_trajectory.py` | 离线工程链 owner | 提供 path bridge 或直接替换默认入口 | `packages/engineering-data`、`packages/process-runtime-core` | 是 |
| `packages/engineering-data` 仍被当作 `dxf_pipeline_path` 默认目录 | 离线工程链 owner | 完成默认目录替换或桥接，保证现有命令面与验证链不失效 | `packages/engineering-data`、`packages/process-runtime-core`、`packages/runtime-host` | 是 |
| `packages/engineering-contracts` 与 `packages/application-contracts` 的 shared 归宿未完全冻结 | 架构治理 owner | 完成 Wave 1 冻结并形成稳定命名与导出方式 | `packages/engineering-data`、`packages/process-runtime-core`、`packages/test-kit`、`integration/*` | 是 |

## 4. Wave 2B Blockers

| 阻塞项 | Owner | 解除条件 | 影响范围 | 是否阻断物理迁移 |
|---|---|---|---|---|
| 根级 build/test 图仍把 `packages/process-runtime-core`、`packages/runtime-host`、`packages/transport-gateway` 当 canonical source root | 构建 owner | 重写根级 CMake / build / test 图并通过全部基线 | `CMakeLists.txt`、`tests/CMakeLists.txt`、`build.ps1`、`test.ps1`、`ci.ps1` | 是 |
| `config/machine/machine_config.ini`、`data/recipes`、`data/schemas/recipes` 仍是 runtime 默认资产路径 | runtime owner | 完成默认路径桥接或切换，并保持 control apps 与 runtime-host 可用 | `apps/*`、`packages/runtime-host`、`packages/transport-gateway` | 是 |
| `process-runtime-core` 仍有 `control-core` residual dependency | legacy migration owner | 逐项清零 `src/shared`、`src/infrastructure`、`modules/device-hal`、`third_party` 等残余依赖 | `packages/process-runtime-core`、`packages/device-adapters`、`packages/device-contracts` | 是 |
| `packages/device-adapters` 与 `packages/device-contracts` 仍受 legacy `device-hal` / `third_party` 阻塞 | device owner | 完成 legacy 实现收口或替换，避免继续从旧目录偷 include/link | `packages/device-adapters`、`packages/device-contracts`、`packages/runtime-host` | 是 |
| `integration/reports` 仍是统一证据根，不能在切换时分裂 | 测试 owner | 新旧目录证据链完全可追踪，并且 CI / legacy-exit / workspace validation 统一接入 | `integration/reports/*`、`.github/workflows/workspace-validation.yml`、`legacy-exit-check.ps1` | 是 |

## 5. 结论

1. `Wave 1` 的阻塞项主要是治理边界未冻结，不是 build/test/CI 失效。
2. `Wave 2A` 的阻塞项主要是 `packages/engineering-data` 的 3 个默认入口。
3. `Wave 2B` 的阻塞项主要是根级构建图、runtime 默认资产路径和 `control-core` residual dependency。
4. 以上三类阻塞项在解除前，均不得启动对应波次的物理迁移。
