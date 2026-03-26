# Quickstart: DSP E2E 全量迁移与 Legacy 清除

## 1. 目标

用最短闭环完成本特性的实施准备，确保迁移归位矩阵、bridge 清除策略、根级 gate 和 closeout 证据在同一套 canonical 口径上工作。

## 2. 必备输入

确认以下输入可读且已冻结到当前分支：

1. 规格文档：`D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-dsp-e2e-full-migration\spec.md`
2. 当前计划产物：`plan.md`、`research.md`、`data-model.md`、`contracts\*`
3. 正式基线：`D:\Projects\SS-dispense-align\docs\architecture\workspace-baseline.md`
4. 正式验收结论：`D:\Projects\SS-dispense-align\docs\architecture\system-acceptance-report.md`
5. 模块冻结参考：`docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s05-module-boundary-interface-contract.md`、`docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s06-repo-structure-guide.md`
6. 当前 gate 入口：`build.ps1`、`test.ps1`、`ci.ps1`、`scripts/validation/run-local-validation-gate.ps1`、`scripts/migration/validate_workspace_layout.py`、`scripts/migration/validate_dsp_e2e_spec_docset.py`、`scripts/migration/legacy-exit-checks.py`

## 2.1 基线解释规则

1. `workspace-baseline.md` 只用于解释 `target canonical roots` 是什么。
2. 旧根当前是否仍存在、是否已经退出，以仓库现实和 `system-acceptance-report.md` 为准。
3. `workspace-baseline.md` 中残留的 `migration-source roots` 表述，不得被当作“本特性仍可保留 live owner 面”的许可。

## 3. 执行顺序

1. 按 `contracts/migration-alignment-clearance-contract.md` 为 `M0-M11`、`modules/*` canonical surface，以及分布在 `apps/`、`shared/`、`docs/`、`samples/`、`tests/`、`scripts/`、`config/`、`data/`、`deploy/` 中与 module owner 直接相关的跨根资产建立迁移归位与清除矩阵。
2. 先补齐验证责任边界并收紧 gate：`validate_workspace_layout.py` 负责 active bridge/live owner graph，`validate_dsp_e2e_spec_docset.py` 负责冻结文档集完整性，`legacy-exit-checks.py` 负责旧根与旧路径退出，`run-local-validation-gate.ps1`/`ci.ps1` 负责聚合。
3. 再逐模块把 bridge/source 子树内容并入标准骨架，同时同步收敛直接相关的 `apps/`、`shared/`、`tests/`、`samples/`、`scripts/` 资产，删除 `src/`、`process-runtime-core/`、`engineering-data/`、`runtime-host/`、`device-adapters/`、`device-contracts/` 和其他桥接 forwarder。
4. 同步更新模块 `README.md`、`module.yaml`、模块级 `CMakeLists.txt`，以及相关 `apps/`、`shared/`、`docs/`、`scripts/`、`tests/` 资产，使 owner 面、构建入口、冻结口径和文档口径一致。
5. 用根级入口补齐 closeout 证据，并将最终结论回写到 canonical docs 和 `tests/reports/`。

## 4. 最低验证命令

在仓库根目录 `D:\Projects\SS-dispense-align\` 执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile Local -Suite contracts
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite contracts -FailOnKnownFailure
python .\scripts\migration\validate_workspace_layout.py --wave "Wave 7"
python .\scripts\migration\validate_dsp_e2e_spec_docset.py --report-dir tests\reports\dsp-e2e-spec-docset-current --report-stem dsp-e2e-spec-docset
python .\scripts\migration\legacy-exit-checks.py --profile local --report-dir tests\reports\legacy-exit-current
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\run-local-validation-gate.ps1
```

Closeout 前追加：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite e2e,protocol-compatibility,performance -FailOnKnownFailure
powershell -NoProfile -ExecutionPolicy Bypass -File .\ci.ps1 -Suite all
```

## 5. 一致性检查

建议在仓库根执行以下检索，确认 bridge/fallback 没有继续出现在 live 口径中：

```powershell
rg -n "bridge_mode:\\s+active|bridge_roots:|SILIGEN_BRIDGE_ROOTS|SILIGEN_MIGRATION_SOURCE_ROOTS" modules scripts
rg -n "process-runtime-core|engineering-data|runtime-host|device-adapters|device-contracts|usecases-bridge" modules apps shared docs scripts tests -g "*.md" -g "*.ps1" -g "*.py" -g "CMakeLists.txt" -g "module.yaml"
rg -n "validate_dsp_e2e_spec_docset|dsp-e2e-spec-docset|SILIGEN_FREEZE_DOCSET_REPORT_DIR" scripts docs tests -g "*.md" -g "*.ps1" -g "*.py"
rg -n "legacy fallback|compatibility shell|bridge root|migration-source roots" docs/architecture tests/reports
```

判定标准：

1. `validate_workspace_layout.py`、根级脚本和 gate 规则中不再以“bridge 存在且受控”为通过条件，live 模块清单和构建脚本中也不再保留 active bridge 状态。
2. `validate_dsp_e2e_spec_docset.py` 报告能够证明 `missing_doc_count=0`、`finding_count=0`，且冻结文档集仍是唯一正式事实源。
3. `legacy-exit-checks.py` 报告能够证明旧顶层根和旧路径文本引用退出，`finding_count=0`。
4. 根级测试与本地 gate 报告能够证明 `known_failure=0`、`overall_status=passed`。

## 6. 进入 `speckit-tasks` 的条件

当以下条件同时满足时，可进入 `speckit-tasks`：

1. `plan.md`、`research.md`、`data-model.md`、`quickstart.md`、`contracts/*` 已完成且无澄清占位。
2. 迁移归位与清除矩阵的字段、状态和 closeout 规则已经冻结，且明确覆盖跨根 owner 资产。
3. 根级验证入口、快速闭环命令、closeout 命令和 docset/layout/legacy-exit 的责任分工已经固定为当前 canonical 口径。
