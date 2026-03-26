# NOISSUE Blocker Register - Wave 2A 后状态

- 状态：Wave 2A Verified / Wave 2B No-Go
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-144819`
- 关联路径清单：`NOISSUE-path-inventory-20260322-144819.md`
- 关联测试计划：`NOISSUE-test-plan-20260322-144819.md`

## 1. Wave 2A Blockers

| 阻塞项 | 当前状态 | 证据 | 是否仍阻断物理迁移 |
|---|---|---|---|
| `packages/engineering-data` 仍绑定 `dxf_to_pb.py` 默认入口 | `Closed` | `DxfPbPreparationService.cpp` 默认值已切到 `tools/engineering-data-bridge/scripts/dxf_to_pb.py` | 否 |
| `packages/engineering-data` 仍绑定 `path_to_trajectory.py` 默认入口 | `Closed` | `IConfigurationPort.h` 默认值已切到 `tools/engineering-data-bridge/scripts/path_to_trajectory.py` | 否 |
| `packages/engineering-data` 仍被当作 `dxf_pipeline_path` 默认目录 | `Closed` | `DXFMigrationConfig.h` 默认值已切到 `tools/engineering-data-bridge` | 否 |
| `packages/engineering-contracts` 与 `packages/application-contracts` 的 shared 归宿未完全冻结 | `Closed` | `Wave 1 Exit = Pass`，`packages` suite 中 `shared-freeze-boundary` 通过 | 否 |

## 2. Wave 2B Blockers

| 阻塞项 | 当前状态 | 证据 | 是否阻断后续物理迁移 |
|---|---|---|---|
| 根级 build/test 图仍把 `packages/process-runtime-core`、`packages/runtime-host`、`packages/transport-gateway` 当 canonical source root | `Open` | `CMakeLists.txt:56/60/80`，`tests/CMakeLists.txt:16/28` | 是 |
| `config/machine/machine_config.ini`、`data/recipes`、`data/schemas/recipes` 仍是 runtime 默认资产路径 | `Open` | `apps/*`、`packages/runtime-host`、`packages/transport-gateway` 的静态命中仍存在 | 是 |
| `process-runtime-core` 仍有 `control-core` residual dependency | `Open` | `packages/process-runtime-core/BOUNDARIES.md`、`packages/process-runtime-core/CMakeLists.txt` | 是 |
| `packages/device-adapters` 与 `packages/device-contracts` 仍受 legacy `device-hal` / `third_party` 阻塞 | `Open` | `packages/device-adapters/README.md`、`apps/hmi-app/DEPENDENCY_AUDIT.md` | 是 |
| `integration/reports` 仍是统一证据根，切换期间不能分裂 | `Open` | 本轮所有收口报告都写入 `integration/reports/verify/wave2a-closeout/*` | 是 |

## 3. 当前结论

1. `Wave 2A` 的 blocker set 已解除，且根级回归已通过
2. 当前仍不能开始 `Wave 2B` 的 build graph、runtime 默认资产路径或 residual dependency 切换
3. 现阶段唯一允许继续推进的是 `Wave 2B` 的证据细化、任务拆解和切换前设计
