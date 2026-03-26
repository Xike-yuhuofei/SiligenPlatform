# Doc Sync Report

- Branch: feat/dispense/NOISSUE-e2e-flow-spec-alignment
- BranchSafe: feat-dispense-NOISSUE-e2e-flow-spec-alignment
- Ticket: NOISSUE
- Timestamp: 20260322-143223
- Scope: Wave 2A path bridge 落地与默认入口替换

## 文档变更列表

1. `packages/process-runtime-core/src/application/usecases/dispensing/README.md`
   - 将 DXF 预处理默认入口从 `packages/engineering-data/scripts/dxf_to_pb.py` 更新为 `tools/engineering-data-bridge/scripts/dxf_to_pb.py`
   - 明确 bridge 当前继续转调 `packages/engineering-data` owner 脚本

2. `docs/process-model/plans/NOISSUE-wave2a-path-bridge-20260322-140814.md`
   - 状态由 `Drafted` 更新为 `Implemented`
   - 明确 bridge 目录和 wrapper 已落地，但物理迁移仍是 No-Go

3. `integration/scenarios/run_engineering_regression.py`
   - 将工程回归脚本中的 `dxf_to_pb.py`、`export_simulation_input.py`、`generate_preview.py` 入口统一切到 `tools/engineering-data-bridge/scripts/*`
   - 保证场景回归不再绕过 Wave 2A bridge

4. `integration/performance/collect_baselines.py`
   - 将性能基线采集中的 `dxf_to_pb.py`、`export_simulation_input.py` 默认入口切到 `tools/engineering-data-bridge/scripts/*`
   - 保持采集行为不变，只收敛稳定调用锚点

## 关键差异摘要

1. `Wave 2A` 已从“设计 bridge”推进到“bridge 已落地，默认入口已替换”。
2. `packages/engineering-data` 仍是实现 owner，本轮没有做物理迁移。
3. 当前变化已从核心默认入口扩展到场景回归与性能采集入口，但仍只影响调用锚点，不改变 override 优先级、runtime 默认资产路径或根级 build/test/CI 接口。

## 待确认项

1. `packages/engineering-data/README.md` 与 `packages/engineering-data/docs/compatibility.md` 仍以 owner 视角描述 `packages/engineering-data/scripts/*`；若要把 bridge 口径写进对外文档，需要单独决定这些文档是保持 owner 视角还是补充 workspace 视角。
