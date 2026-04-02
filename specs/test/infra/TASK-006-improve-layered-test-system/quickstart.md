# Quickstart: Layered Test System

## 1. 目标

用最短闭环开始实施“分层测试体系”，确保新增或调整的测试能够被正确归入验证层、复用共享资产与夹具、通过根级入口执行，并生成统一证据。

## 2. 必备输入

开始前确认以下资产存在：

1. feature 规格与计划
   `D:\Projects\ss-e\specs\test\infra\TASK-006-improve-layered-test-system\spec.md`
   `D:\Projects\ss-e\specs\test\infra\TASK-006-improve-layered-test-system\plan.md`
   `D:\Projects\ss-e\specs\test\infra\TASK-006-improve-layered-test-system\research.md`
   `D:\Projects\ss-e\specs\test\infra\TASK-006-improve-layered-test-system\data-model.md`
   `D:\Projects\ss-e\specs\test\infra\TASK-006-improve-layered-test-system\contracts\*`
2. 根级验证与说明
   `D:\Projects\ss-e\build.ps1`
   `D:\Projects\ss-e\test.ps1`
   `D:\Projects\ss-e\ci.ps1`
   `D:\Projects\ss-e\docs\architecture\build-and-test.md`
   `D:\Projects\ss-e\docs\validation\README.md`
3. 共享测试支撑
   `D:\Projects\ss-e\shared\testing\README.md`
   `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\`
4. 仓库级承载面
   `D:\Projects\ss-e\tests\integration\README.md`
   `D:\Projects\ss-e\tests\e2e\README.md`
   `D:\Projects\ss-e\tests\performance\README.md`

## 3. 最小准备

在仓库根目录 `D:\Projects\ss-e\` 执行：

```powershell
.\build.ps1 -Profile Local -Suite all
.\test.ps1 -Profile Local -Suite contracts,protocol-compatibility
python .\scripts\migration\validate_workspace_layout.py --wave "Wave 7"
.\scripts\validation\run-local-validation-gate.ps1
```

这一步的目的不是完成全部回归，而是先确认当前 root entry、workspace validation 和 local gate 都可用。

## 4. 第一条实施切片

建议按以下顺序落第一条分层测试切片：

1. 选择一个明确 owner 和一个主验证层
   例如 `motion-planning` 的 `L1-module-contract`，或 `runtime-execution` 的 `L3-simulated-e2e`
2. 为该切片补齐事实资产
   样本输入放 `samples/`，golden baseline 放 `tests/baselines/`，人工或联机核对清单放 `docs/validation/`
3. 如果需要跨模块复用的 fake、clock、report helper
   把它们放到 `shared/testing/test-kit/` 或 `shared/testing/`，不要塞回业务 owner 层
4. 按主验证层落测试
   模块契约落各自 `*/tests`，跨模块链路落 `tests/integration/`，模拟全链路落 `tests/e2e/simulated-line/`，性能画像落 `tests/performance/`
5. 为测试补齐 evidence 输出
   让结论最终回收到 `tests/reports/`，并包含必要 trace 字段

## 5. 按通道执行验证

### Quick Gate

用于常规改动和快速门禁：

```powershell
.\build.ps1 -Profile Local -Suite all
.\test.ps1 -Profile Local -Suite contracts,protocol-compatibility
.\scripts\validation\run-local-validation-gate.ps1
```

### Full Offline Gate

用于跨模块链路、模拟全链路与回归：

```powershell
.\test.ps1 -Profile CI -Suite contracts,e2e,protocol-compatibility -FailOnKnownFailure
.\ci.ps1 -Suite all
```

### Performance Lane

用于 nightly-performance threshold gate 与漂移观测：

```powershell
python .\tests\performance\collect_dxf_preview_profiles.py `
  --sample-labels small `
  --cold-iterations 1 `
  --hot-warmup-iterations 1 `
  --hot-iterations 1 `
  --singleflight-rounds 1 `
  --singleflight-fanout 3 `
  --gate-mode nightly-performance `
  --threshold-config .\tests\baselines\performance\dxf-preview-profile-thresholds.json
```

说明：

- `--gate-mode nightly-performance` + `--threshold-config` 是正式 nightly blocking path。
- `--baseline-json` 仍只用于 advisory drift compare，不替代 threshold gate。

### Limited HIL

只在离线层通过后执行，正式受控路径固定走 `run_hil_controlled_test.ps1`：

```powershell
.\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 `
  -Profile Local `
  -UseTimestampedReportDir `
  -PublishLatestOnPass:$false
```

说明：

- 该路径会按 `offline-prereq -> hardware-smoke -> hil-closed-loop -> optional hil-case-matrix -> gate -> release-summary` 顺序执行。
- 根级 `test.ps1 -IncludeHardwareSmoke/-IncludeHil*` 与 `ci.ps1 -IncludeHilCaseMatrix` 只保留为 opt-in surface，不作为正式 controlled HIL release path。
- 只有在离线前置层未完全通过、且现场负责人明确批准时，才允许额外传 `-OperatorOverrideReason "<reason>"`。

## 6. 完成判定

实施完成后的最小通过标准：

1. 新增或改造的测试已明确绑定 `ValidationLayer`、`owner_scope` 和 `ExecutionLane`
2. 所有共享资产与共享 fixture 都位于 canonical roots
3. 默认验证路径保持离线优先，没有把 HIL 变成默认 gate
4. `tests/reports/` 中能看到与该切片对应的结构化证据
5. 不稳定测试被显式标记，没有被当作可信门禁

## 7. 进入 `speckit-tasks` 的条件

满足以下条件后即可进入 `speckit-tasks`：

1. `plan.md`、`research.md`、`data-model.md`、`quickstart.md`、`contracts/*` 已冻结
2. 分层、路由、共享资产、证据 bundle 和 HIL 边界已经明确
3. 所有设计产物都继续依赖 canonical roots 与 root entry points，没有留下 `NEEDS CLARIFICATION`
