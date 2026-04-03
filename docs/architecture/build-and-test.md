# Build And Test

更新时间：`2026-04-03`

## 根级正式入口

```powershell
.\build.ps1
.\test.ps1 -Profile Local -Suite all
.\ci.ps1 -Suite all
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\run-local-validation-gate.ps1
```

统一规则：

- 正式入口只认 `build.ps1`、`test.ps1`、`ci.ps1`
- 根级入口统一接受 `-Lane`、`-RiskProfile`、`-DesiredDepth`、`-ChangedScope`、`-SkipLayer`、`-SkipJustification`
- `limited-hil` 正式入口使用 `-IncludeHilClosedLoop`、`-IncludeHilCaseMatrix` 控制 `L5` 闭环与矩阵资产
- `limited-hil` 的 offline prerequisites 必须先覆盖 `contracts + integration + e2e + protocol-compatibility`，以满足 `engineering-regression` / `simulated-line` / `protocol-compatibility` admission 契约
- `python -m test_kit.workspace_validation` 是根级测试聚合 authority
- `tests/reports/static/` 是静态门禁正式报告根

## 正式分层

全仓只允许以下术语：

- `L0`：静态与构建门禁
- `L1`：单元测试
- `L2`：模块契约测试
- `L3`：集成测试
- `L4`：模拟全链路 E2E
- `L5`：HIL 闭环测试
- `L6`：性能与稳定性测试

详细说明见：

- `docs/architecture/layered-test-system-v2.md`
- `docs/architecture/test-coverage-matrix-v1.md`
- `docs/architecture/machine-preflight-quality-gate-v1.md`

## lane 映射

| lane | 正式覆盖 |
|---|---|
| `quick-gate` | `L0` + `L1` 核心单元 + `L2` 核心契约 |
| `full-offline-gate` | `L0-L4` |
| `nightly-performance` | `L6`，必要时复用 `L3/L4` 样本 |
| `limited-hil` | `L5`，必要时附带少量 `L6` 样本 |

`nightly-performance` 当前默认由 `python -m test_kit.workspace_validation --lane nightly-performance --suite performance` 接到 `tests/performance/collect_dxf_preview_profiles.py`，并固定追加：

- `--include-start-job`
- `--include-control-cycles`
- `--pause-resume-cycles 3`
- `--stop-reset-rounds 3`
- `--long-run-minutes 5`
- `--gate-mode nightly-performance`
- `--threshold-config tests/baselines/performance/dxf-preview-profile-thresholds.json`

这条 lane 的 blocking 判定只认 `threshold_gate`，不再把 `baseline-json` 漂移对比当作正式门禁。

## 当前 L0 authority

- `pyrightconfig.json`
- `scripts/validation/run-pyright-gate.ps1`
- `tools/testing/check_no_loose_mock.py`

`L0` 失败即阻断 `quick-gate`，并禁止继续进入 `L3/L4/L5`。

## 证据输出

| 入口 / lane | 默认输出 |
|---|---|
| static gates | `tests/reports/static/pyright-report.*`, `tests/reports/static/no-loose-mock-report.*` |
| `test.ps1` | `tests/reports/workspace-validation.json`, `tests/reports/workspace-validation.md` |
| `ci.ps1` | `tests/reports/ci/` |
| `run-local-validation-gate.ps1` | `tests/reports/local-validation-gate/<timestamp>/` |
| integration / e2e | `case-index.json`, `validation-evidence-bundle.json`, `evidence-links.md` |
| `nightly-performance` | `tests/reports/performance/dxf-preview-profiles/` |
| `limited-hil` | `tests/reports/hil-controlled-test/**` |

`nightly-performance` 报告除 `Preview / Execution / Single Flight` 外，还必须包含：

- `Control Cycle` 表：`pause/resume`、`stop/reset/rerun` 时延与资源增量
- `Long Run` 表：长稳执行时延、内存/句柄/线程趋势与 timeout 计数
