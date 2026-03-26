# NOISSUE Wave 1 阶段出口清单

- 状态：Ready for Exit Review
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 关联阶段计划：NOISSUE-stage-plan-20260322-134203.md
- 关联基线状态：NOISSUE-baseline-status-20260322-134203.md
- 关联 shared 边界文档：docs/architecture/shared-boundary-freeze-wave1.md

## 1. 出口目标

本清单只回答一个问题：`Wave 1` 是否已经完成到“可以开始 Wave 2A 设计与 bridge 准备”，但仍不能开始物理迁移。

## 2. 必须满足的事实

1. 基线证据为绿：
   - `legacy-exit` 通过
   - `apps/packages/integration/protocol-compatibility/simulation` 无 `new failure`
2. `shared/*` 目标边界已冻结：
   - `shared/contracts/application`
   - `shared/contracts/device`
   - `shared/contracts/engineering`
   - `shared/kernel`
   - `shared/testing`
   - `shared/logging`
3. `Wave 1` 轻量门禁已落地并接入默认 `packages` suite：
   - `tools/migration/validate_shared_freeze.py`
   - `tools/migration/shared_freeze_manifest.json`
   - `packages/test-kit/src/test_kit/workspace_validation.py`
4. `shared/` 目标骨架已进入工作树，但未宣称实现已迁移。
5. `Wave 2A` 与 `Wave 2B` 的阻塞项仍被保留，没有被提前删除或弱化。

## 3. 必须保持不变的事项

1. 根级 `build.ps1`、`test.ps1`、`ci.ps1` 的调用接口未改。
2. `config/machine/machine_config.ini`、`data/recipes`、`data/schemas/recipes` 仍是默认路径。
3. `apps/control-runtime`、`apps/control-tcp-server`、`apps/control-cli` 的 dry-run 仍指向 canonical。
4. `Wave 1` 轻量门禁只约束 shared 冻结，不接管 runtime 行为或 build graph。

## 4. 当前出口判断

### 4.1 Go

1. 可以进入 `Wave 2A` 的 path bridge 设计与实现准备。
2. 可以继续补 shared/testing、shared/logging、contracts fixtures 的治理拆账。
3. 可以继续强化 `packages` suite 的治理性门禁。

### 4.2 No-Go

1. 不可开始 `packages/engineering-data` 的物理迁移。
2. 不可开始 `packages/process-runtime-core`、`packages/runtime-host`、`packages/transport-gateway` 的目录翻转。
3. 不可改写 runtime 默认资产路径。
4. 不可把 `shared/*` 写成已完成迁移。

## 5. Wave 1 出口结论

当前结论：`Wave 1 Exit = Pass`

含义：

1. `Wave 1` 的治理落地已经完成。
2. 下一步应进入 `Wave 2A` 的 path bridge 设计与默认入口替换准备。
3. `Wave 2A` 的进入条件仍需单独满足，不能由 `Wave 1 Exit = Pass` 自动推导为可物理迁移。
