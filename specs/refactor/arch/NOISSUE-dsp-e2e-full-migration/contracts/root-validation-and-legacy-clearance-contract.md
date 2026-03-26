# Contract: 根级验证与 Legacy 清除

## 1. 合同标识

- Contract ID: `root-validation-legacy-clearance-v1`
- Scope: `build.ps1`, `test.ps1`, `ci.ps1`, `scripts/validation/run-local-validation-gate.ps1`, `scripts/migration/validate_workspace_layout.py`, `scripts/migration/validate_dsp_e2e_spec_docset.py`, `scripts/migration/legacy-exit-checks.py`

## 2. 目的

定义本特性 closeout 所要求的根级验证入口、失败条件、报告输出和 zero-fallback 约束，确保 bridge/source/fallback 清除后，工作区仍能通过单轨 canonical graph 完成构建、测试和门禁验证。

## 3. 正式入口

| 入口 | 作用 | 必须证明 |
|---|---|---|
| `build.ps1` | 根级构建验证入口 | canonical build graph 可独立构建，不依赖 bridge/fallback |
| `test.ps1` | 根级测试入口 | contracts/e2e/protocol-compatibility/performance 在 canonical graph 上通过 |
| `ci.ps1` | 根级 closeout 入口 | 单次执行即可串联 formal gateway、legacy-exit、build、test、local gate |
| `scripts/validation/run-local-validation-gate.ps1` | 本地冻结门禁 | workspace layout、formal docset、contracts build/test 等本地硬门槛通过 |
| `scripts/migration/validate_workspace_layout.py` | 工作区布局校验 | 模块骨架、正式根、active bridge/live owner graph/bridge metadata 违规被拒绝 |
| `scripts/migration/validate_dsp_e2e_spec_docset.py` | 冻结文档集校验 | `dsp-e2e-spec` 仍然完整、可用且满足当前 acceptance 轴 |
| `scripts/migration/legacy-exit-checks.py` | legacy 退出校验 | 旧顶层根和旧路径文本引用为零 |

## 4. 必须失败的情况

以下任一情况出现时，相关入口必须返回失败：

1. 模块 `module.yaml` 仍保留 `bridge_mode: active`
2. 模块 `CMakeLists.txt` 仍保留 `SILIGEN_BRIDGE_ROOTS` 或 `SILIGEN_MIGRATION_SOURCE_ROOTS`
3. `modules/*/src`、`modules/workflow/process-runtime-core`、`modules/dxf-geometry/engineering-data`、`modules/runtime-execution/runtime-host`、`modules/runtime-execution/device-adapters`、`modules/runtime-execution/device-contracts` 等 bridge 根仍存在于 live owner graph 中
4. 根级脚本、模块脚本、README 或 gate 规则仍将 bridge/fallback 路径描述为默认入口
5. `validate_workspace_layout.py` 或根级 gate 仍以“bridge 存在且受控”为通过条件
6. `validate_dsp_e2e_spec_docset.py` 发现缺轴、缺章节、缺 acceptance 关键术语，或 docset 报告缺失

## 5. 报告要求

| 入口 | 最低报告要求 |
|---|---|
| `test.ps1` | 产出 `tests/reports/**/workspace-validation.md` |
| `ci.ps1` | 产出 `tests/reports/**/workspace-validation.md`、`tests/reports/**/legacy-exit-checks.md`、`tests/reports/**/local-validation-gate-summary.md`，并携带 `dsp-e2e-spec-docset/` 报告目录 |
| `run-local-validation-gate.ps1` | 产出 `local-validation-gate-summary.json` 与 `local-validation-gate-summary.md` |
| `validate_dsp_e2e_spec_docset.py` | 产出 `dsp-e2e-spec-docset.json` 与 `dsp-e2e-spec-docset.md` |
| `legacy-exit-checks.py` | 产出 `legacy-exit-checks.json` 与 `legacy-exit-checks.md` |

报告中必须按入口类型明确看到以下结论：

- `test.ps1` / workspace validation 报告：`known_failure=0`
- `legacy-exit-checks.py` 报告：`finding_count=0`
- `run-local-validation-gate.ps1` 报告：`overall_status=passed`
- `validate_dsp_e2e_spec_docset.py` 报告：`missing_doc_count=0` 且 `finding_count=0`
- 聚合结论中不得出现 bridge/fallback/legacy root 命中

## 6. 推荐执行层次

### 快速闭环

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile Local -Suite contracts
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite contracts -FailOnKnownFailure
python .\scripts\migration\validate_workspace_layout.py --wave "Wave 7"
python .\scripts\migration\validate_dsp_e2e_spec_docset.py --report-dir tests\reports\dsp-e2e-spec-docset-current --report-stem dsp-e2e-spec-docset
python .\scripts\migration\legacy-exit-checks.py --profile local --report-dir tests\reports\legacy-exit-current
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\run-local-validation-gate.ps1
```

### Closeout

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite e2e,protocol-compatibility,performance -FailOnKnownFailure
powershell -NoProfile -ExecutionPolicy Bypass -File .\ci.ps1 -Suite all
```

## 7. 合同边界

- 允许保留 `docs/_archive/`、历史报告目录和 support/vendor/generated roots 作为审计资产，但这些资产不得成为 live owner graph 或 root gate 默认入口的一部分。
- 允许协议兼容测试继续存在，但它们必须验证 canonical consumer，而不是重新启用 bridge shell。
