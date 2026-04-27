# NOISSUE Wave 3C Test Plan - control-cli Workspace Entry Tightening + Engineering-Data Bridge Normalization

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-202337`
- 关联 scope：`docs/process-model/plans/NOISSUE-wave3c-scope-20260322-202337.md`
- 关联 arch：`docs/process-model/plans/NOISSUE-wave3c-arch-plan-20260322-202337.md`

## 1. 验证原则

1. 同一 control-apps build root 上所有构建与测试串行执行。
2. 先跑静态/兼容 gate，再跑 CLI dry-run 与 app build，最后跑 app/protocol 回归。
3. 只验证 Wave 3C 的入口规范化，不把仓外兼容窗口退出混入本批次。

## 2. 静态与兼容验证

### 2.1 engineering-data canonical/legacy 兼容

```powershell
python packages/engineering-data/tests/test_engineering_data_compatibility.py
python packages/engineering-data/tests/test_dxf_pipeline_legacy_shims.py
```

预期：

1. canonical API/CLI 兼容测试继续通过。
2. legacy import/CLI shim 继续存在且只做 canonical 转发。

### 2.2 scoped 负向搜索

```powershell
rg -n "packages/engineering-data/scripts/(generate_preview|dxf_to_pb|export_simulation_input|path_to_trajectory)\.py" apps integration packages tools --glob "!docs/process-model/**"
rg -n "current_path\(" apps/control-cli
```

预期：

1. live code 中不再把 owner 脚本当成 workspace 默认入口，仅允许 README/owner 文档说明保留 owner 脚本列表。
2. `apps/control-cli` 不再残留 cwd 猜测式解析。

## 3. CLI / apps 验证

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File apps/control-cli/run.ps1 -DryRun
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite apps
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\wave3c-closeout\apps -FailOnKnownFailure
```

预期：

1. `run.ps1 -DryRun` 继续只解析 canonical `siligen_cli.exe`。
2. `control-cli-bootstrap-check-subdir`、`control-cli-bootstrap-check-compat-alias-rejected` 继续通过。
3. `control-cli-dxf-preview` 与 `control-cli-dxf-preview-subdir` 在 bridge 默认脚本入口下继续通过。

## 4. 集成与协议回归

```powershell
python integration/scenarios/run_engineering_regression.py
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite protocol-compatibility -ReportDir integration\reports\verify\wave3c-closeout\protocol -FailOnKnownFailure
```

预期：

1. 工程回归默认走 bridge 脚本入口，但输出契约不变。
2. protocol-compatibility suite 继续全绿，不引入新的 failed/known_failure。
