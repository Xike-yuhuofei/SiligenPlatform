# NOISSUE 基线状态 - 整仓目录重构前置准备（适配版候选骨架）

- 状态：已采集基线
- 日期：2026-03-22
- 时间戳：20260322-134203
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 关联测试计划：NOISSUE-test-plan-20260322-130211.md

## 1. 结论

本次基线采集已完成，`apps`、`packages`、`integration`、`protocol-compatibility`、`simulation`、`legacy-exit` 六类验证均无 `new failure`，并且 app dry-run 结果继续指向 canonical 来源。

当前状态足以支撑下一阶段治理收口和 `Wave 1` 的边界冻结，但不支持进入物理目录迁移。

## 2. 基线结果

### 2.1 `apps`

- 命令：
  - `apps/control-runtime/run.ps1 -DryRun`
  - `apps/control-tcp-server/run.ps1 -DryRun`
  - `apps/control-cli/run.ps1 -DryRun`
  - `.\build.ps1 -Profile CI -Suite apps`
  - `.\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\dir-refactor-prep\apps -FailOnKnownFailure`
- 结果：
  - `control-runtime`、`control-tcp-server`、`control-cli` dry-run 均输出 `source: canonical`
  - `apps` suite 通过，`passed=16 failed=0 known_failure=0`
- 报告目录：
  - `integration/reports/verify/dir-refactor-prep/apps`
- 结论：
  - app 外部入口基线稳定，未发现 legacy fallback 回流

### 2.2 `packages`

- 命令：
  - `.\test.ps1 -Profile CI -Suite packages -ReportDir integration\reports\verify\dir-refactor-prep\packages -FailOnKnownFailure`
- 结果：
  - `packages` suite 通过，`passed=5 failed=0 known_failure=0`
- 报告目录：
  - `integration/reports/verify/dir-refactor-prep/packages`
- 结论：
  - `application-contracts`、`engineering-contracts`、`engineering-data`、`transport-gateway` 的基础验证维持通过

### 2.3 `integration`

- 命令：
  - `.\test.ps1 -Profile CI -Suite integration -ReportDir integration\reports\verify\dir-refactor-prep\integration -FailOnKnownFailure`
- 结果：
  - `integration` suite 通过，`passed=1 failed=0 known_failure=0`
- 报告目录：
  - `integration/reports/verify/dir-refactor-prep/integration`
- 结论：
  - 工程回归链路未回退

### 2.4 `protocol-compatibility`

- 命令：
  - `.\test.ps1 -Profile CI -Suite protocol-compatibility -ReportDir integration\reports\verify\dir-refactor-prep\protocol -FailOnKnownFailure`
- 结果：
  - `protocol-compatibility` suite 通过，`passed=1 failed=0 known_failure=0`
- 报告目录：
  - `integration/reports/verify/dir-refactor-prep/protocol`
- 结论：
  - 应用/工程/传输契约兼容性保持稳定

### 2.5 `simulation`

- 命令：
  - `.\build.ps1 -Profile CI -Suite simulation`
  - `.\test.ps1 -Profile CI -Suite simulation -ReportDir integration\reports\verify\dir-refactor-prep\simulation -FailOnKnownFailure`
- 结果：
  - `simulation` build 通过
  - `simulation` suite 通过，`passed=5 failed=0 known_failure=0`
- 报告目录：
  - `integration/reports/verify/dir-refactor-prep/simulation`
- 结论：
  - 仿真构建与仿真回归链路稳定

### 2.6 `legacy-exit`

- 命令：
  - `.\legacy-exit-check.ps1 -Profile CI -ReportDir integration\reports\verify\dir-refactor-prep\legacy-exit`
- 结果：
  - `passed_rules=18 failed_rules=0`
- 报告目录：
  - `integration/reports/verify/dir-refactor-prep/legacy-exit`
- 结论：
  - 现有 legacy 回流门禁保持绿色，未发现新增失败规则

## 3. 总体判断

1. 本次基线采集结果可作为后续目录重构准备阶段的冻结证据。
2. 现阶段可以继续推进 `Wave 1` 的 `shared/*` 边界冻结和治理落地。
3. 现阶段不应进入物理目录迁移，尤其不能提前触碰 `packages/engineering-data`、`packages/process-runtime-core`、`packages/runtime-host`、`packages/transport-gateway` 的执行绑定。
