# NOISSUE Wave 2A 收口报告

- 状态：Pass
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-144819`
- 关联设计：`NOISSUE-wave2a-path-bridge-20260322-140814.md`
- 关联报告根：`integration/reports/verify/wave2a-closeout/`

## 1. 收口范围

本次只收口以下事实，不启动任何物理迁移：

1. `DxfPbPreparationService` 默认 `.pb` 预处理入口切到 `tools/engineering-data-bridge/scripts/dxf_to_pb.py`
2. `DxfTrajectoryConfig.script` 默认轨迹脚本入口切到 `tools/engineering-data-bridge/scripts/path_to_trajectory.py`
3. `DXFMigrationConfig.dxf_pipeline_path` 默认目录切到 `tools/engineering-data-bridge`
4. `integration/scenarios/run_engineering_regression.py` 与 `integration/performance/collect_baselines.py` 默认工作区脚本入口切到 `tools/engineering-data-bridge/scripts/*`
5. 文档口径改为“bridge 是稳定路径锚点，`packages/engineering-data` 仍是实现 owner”

## 2. 当前代码事实

1. `packages/process-runtime-core/src/application/services/dxf/DxfPbPreparationService.cpp`
   - 默认脚本回退路径已是 `tools/engineering-data-bridge/scripts/dxf_to_pb.py`
2. `packages/process-runtime-core/src/domain/configuration/ports/IConfigurationPort.h`
   - 默认轨迹脚本路径已是 `tools/engineering-data-bridge/scripts/path_to_trajectory.py`
3. `packages/process-runtime-core/src/infrastructure/adapters/planning/dxf/DXFMigrationConfig.h`
   - `dxf_pipeline_path` 默认值已是 `tools/engineering-data-bridge`
4. `tools/engineering-data-bridge/scripts/`
   - 已提供 `dxf_to_pb.py`、`path_to_trajectory.py`、`export_simulation_input.py`、`generate_preview.py`
   - 当前全部仅做 wrapper，继续转调 `packages/engineering-data/scripts/*`
5. `packages/engineering-data`
   - 仍是当前实现 owner，没有做物理迁移、删除或重命名

## 3. 验证结果

### 3.1 app dry-run

1. `apps/control-runtime/run.ps1 -DryRun`
   - 结果：`source: canonical`
2. `apps/control-tcp-server/run.ps1 -DryRun`
   - 结果：`source: canonical`
3. `apps/control-cli/run.ps1 -DryRun`
   - 结果：`source: canonical`

### 3.2 legacy-exit

1. 命令：`.\legacy-exit-check.ps1 -Profile CI -ReportDir integration\reports\verify\wave2a-closeout\legacy-exit`
2. 结果：`passed_rules=18`，`failed_rules=0`

### 3.3 根级 suites

1. `.\build.ps1 -Profile CI -Suite apps`
   - 结果：通过
2. `.\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\wave2a-closeout\apps -FailOnKnownFailure`
   - 结果：`passed=16 failed=0 known_failure=0`
3. `.\test.ps1 -Profile CI -Suite packages -ReportDir integration\reports\verify\wave2a-closeout\packages -FailOnKnownFailure`
   - 结果：`passed=6 failed=0 known_failure=0`
4. `.\test.ps1 -Profile CI -Suite integration -ReportDir integration\reports\verify\wave2a-closeout\integration -FailOnKnownFailure`
   - 结果：`passed=1 failed=0 known_failure=0`
5. `.\test.ps1 -Profile CI -Suite protocol-compatibility -ReportDir integration\reports\verify\wave2a-closeout\protocol -FailOnKnownFailure`
   - 结果：`passed=1 failed=0 known_failure=0`
6. `.\build.ps1 -Profile CI -Suite simulation`
   - 结果：通过
7. `.\test.ps1 -Profile CI -Suite simulation -ReportDir integration\reports\verify\wave2a-closeout\simulation -FailOnKnownFailure`
   - 结果：`passed=5 failed=0 known_failure=0`

## 4. 结论

1. `Wave 2A Closeout = Pass`
2. 当前工作树已满足“默认入口切到 bridge、owner 不变、根级门禁无新增失败”的最小验收
3. 下一阶段只允许进入 `Wave 2B` 取证与拆解，不允许因为 `Wave 2A` 已通过就直接开始 `packages/engineering-data` 物理迁移

## 5. 明确 No-Go

1. 不可删除、移动或重命名 `packages/engineering-data`
2. 不可改写 `build.ps1`、`test.ps1`、`ci.ps1` 的公共接口
3. 不可把 runtime 默认资产路径切换到新目录
4. 不可把 `integration/reports` 分裂成新的证据根
