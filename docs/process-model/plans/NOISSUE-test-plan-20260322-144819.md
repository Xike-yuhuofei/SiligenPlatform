# NOISSUE 测试计划 - Wave 2A 收口 / Wave 2B 取证

- 状态：Executed for Wave 2A，Prepared for Wave 2B
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-144819`
- 关联收口报告：`NOISSUE-wave2a-closeout-20260322-144819.md`

## 1. 目标

1. 用根级命令确认 `Wave 2A` 的 bridge 默认入口替换没有引入回归
2. 用静态搜索确认 `Wave 2B` 的阻塞点仍真实存在，并且证据链可追溯

## 2. Wave 2A 根级回归命令

1. `apps/control-runtime/run.ps1 -DryRun`
2. `apps/control-tcp-server/run.ps1 -DryRun`
3. `apps/control-cli/run.ps1 -DryRun`
4. `.\legacy-exit-check.ps1 -Profile CI -ReportDir integration\reports\verify\wave2a-closeout\legacy-exit`
5. `.\build.ps1 -Profile CI -Suite apps`
6. `.\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\wave2a-closeout\apps -FailOnKnownFailure`
7. `.\test.ps1 -Profile CI -Suite packages -ReportDir integration\reports\verify\wave2a-closeout\packages -FailOnKnownFailure`
8. `.\test.ps1 -Profile CI -Suite integration -ReportDir integration\reports\verify\wave2a-closeout\integration -FailOnKnownFailure`
9. `.\test.ps1 -Profile CI -Suite protocol-compatibility -ReportDir integration\reports\verify\wave2a-closeout\protocol -FailOnKnownFailure`
10. `.\build.ps1 -Profile CI -Suite simulation`
11. `.\test.ps1 -Profile CI -Suite simulation -ReportDir integration\reports\verify\wave2a-closeout\simulation -FailOnKnownFailure`

## 3. Wave 2A 执行结果

1. 三个 app dry-run 全部输出 `source: canonical`
2. `legacy-exit`：`passed_rules=18 failed_rules=0`
3. `apps`：`passed=16 failed=0 known_failure=0`
4. `packages`：`passed=6 failed=0 known_failure=0`
5. `integration`：`passed=1 failed=0 known_failure=0`
6. `protocol-compatibility`：`passed=1 failed=0 known_failure=0`
7. `simulation`：`passed=5 failed=0 known_failure=0`

## 4. Wave 2B 静态取证命令

1. `rg -n "packages/process-runtime-core|packages/runtime-host|packages/transport-gateway|config/machine/machine_config.ini|third_party" CMakeLists.txt tests/CMakeLists.txt build.ps1 test.ps1 ci.ps1 tools/build tools/scripts .github/workflows`
2. `rg -n "config/machine/machine_config.ini|data/recipes|data/schemas/recipes" apps packages/runtime-host packages/transport-gateway`
3. `rg -n "control-core/src/shared|control-core/src/infrastructure|control-core/modules/device-hal|control-core/third_party|device-hal|third_party" packages/process-runtime-core packages/device-adapters packages/device-contracts apps/hmi-app`

## 5. 通过 / 阻断标准

1. 以下任一项不满足，则 `Wave 2A` 不得判定为收口通过：
   - 任一 app dry-run 不再输出 canonical
   - `legacy-exit` 出现新增失败
   - 任一 suite 出现 `failed > 0` 或新增 known failure
   - 默认入口仍直接写死 `packages/engineering-data/scripts/dxf_to_pb.py`
   - 默认入口仍直接写死 `packages/engineering-data/scripts/path_to_trajectory.py`
   - `DXFMigrationConfig.dxf_pipeline_path` 默认值仍是 `packages/engineering-data`
2. 以下任一项存在，则 `Wave 2B` 维持 `No-Go`：
   - 根级 build/test/source-root 仍把 `packages/process-runtime-core`、`packages/runtime-host`、`packages/transport-gateway` 当 canonical root
   - runtime 默认资产路径仍是 `config/machine/machine_config.ini`、`data/recipes`、`data/schemas/recipes`
   - `process-runtime-core` 对 `control-core` residual dependency 仍未清零
   - `device-adapters` / `device-contracts` 仍受 legacy `device-hal` / `third_party` 阻塞
   - `integration/reports` 统一证据根尚无可替代方案

## 6. 结论

1. `Wave 2A` 根级回归已执行并通过
2. `Wave 2B` 本轮只允许继续做证据拆解、子任务分配和切换前设计，不允许执行 build graph 或默认路径切换
