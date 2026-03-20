# Validation

工作区级验证入口已经统一到根级脚本：

- [build.ps1](/D:/Projects/SiligenSuite/build.ps1)
- [test.ps1](/D:/Projects/SiligenSuite/test.ps1)
- [ci.ps1](/D:/Projects/SiligenSuite/ci.ps1)
- [legacy-exit-check.ps1](/D:/Projects/SiligenSuite/legacy-exit-check.ps1)

## 评审基线

- [工程替身系统总纲说明页](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-charter.md)
- [需求定义稿](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-demand-definition.md)
- [非目标边界](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-non-goals.md)
- [成立条件稿](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-acceptance-conditions.md)
- [范围定义稿](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-scope-definition.md)
- [核心范围分层稿](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-core-scope-layering.md)
- [首层场景筛选稿](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-scene-screening.md)
- [首层业务场景逐项说明稿](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-scene-details.md)
- [场景-成立条件映射稿](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-scene-condition-mapping.md)
- [验收口径稿](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-acceptance-criteria.md)
- [正式评审执行包冻结稿](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-package-freeze-baseline-v1.0.md)
- [证据补齐与首轮复审准备稿](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-evidence-remediation-and-rereview-prep.md)
- [逐场景证据采集与复审输入包组装稿](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-scene-evidence-collection-and-rereview-package-assembly.md)
- [首轮复审输入包总索引页](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-rereview-package-index.md)
- [正式评审执行清单](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-execution-checklist.md)
- [首层成立性评审包封面页 Baseline v1.0](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-package-cover-baseline-v1.0.md)
- [首层评审会议议程页](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-meeting-agenda.md)
- [S1-S9 首轮评审证据准备表](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-evidence-prep-table.md)
- [S1-S9 首轮评审记录总索引页](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-record-index.md)
- [首层结论汇总](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-summary.md)
- [阻断结论总表](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-blocker-matrix.md)

## 分层位置

- legacy 删除 / 冻结门禁：`.\legacy-exit-check.ps1`
- control apps 可执行产物：`<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_control_runtime.exe`、`siligen_tcp_server.exe`、`siligen_cli.exe`
- `apps` suite 会同时执行三个 `run.ps1 -DryRun` 与真实 exe 验证：`siligen_control_runtime.exe --version`、`siligen_tcp_server.exe --help`、`siligen_cli.exe bootstrap-check`
- `apps` suite 也会执行 `apps\hmi-app\run.ps1 -DryRun -DisableGatewayAutostart`、`apps\hmi-app\scripts\test.ps1`、`apps\hmi-app\scripts\offline-smoke.ps1`、`apps\hmi-app\scripts\online-smoke.ps1` 与 `apps\hmi-app\scripts\verify-online-ready-timeout.ps1`
- HMI smoke 退出码与失败注入约定见 `apps\hmi-app\docs\smoke-exit-codes.md`
- `process-runtime-core` 单测产物：`<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_unit_tests.exe`、`siligen_pr1_tests.exe`
  其中测试源码 canonical 位置已经收口到 `packages/process-runtime-core/tests/`，`control-core/tests/CMakeLists.txt` 当前仅保留兼容转发入口，产物已经随 canonical control-apps build root 输出
- 仿真单测：`packages/simulation-engine/build/Debug/*.exe`
- 集成：`integration/protocol-compatibility/`、`integration/scenarios/`
- 仿真/HIL：`integration/simulated-line/`、`integration/hardware-in-loop/`

`CONTROL_APPS_BUILD_ROOT` 解析顺序：

- `SILIGEN_CONTROL_APPS_BUILD_ROOT`
- `%LOCALAPPDATA%\SiligenSuite\control-apps-build`
- `D:\Projects\SiligenSuite\build\control-apps`

当前 fallback 口径：

- `control-runtime`、`control-tcp-server` 的 `run.ps1` 已移除 legacy fallback
- `control-cli` 只验证 canonical `siligen_cli.exe`，不再回退到 legacy CLI 二进制
- `control-core/config/*`、`control-core/data/recipes/*`、`control-core/src/infrastructure/resources/config/files/recipes/schemas/*` 已退出默认验证链路
- legacy gateway/tcp alias 已删除；`legacy-exit-check.ps1` 会拦截其回流到 `control-core` 注册

## 报告

根级验证报告输出到 `integration/reports/`。

- `legacy-exit-check.ps1` 输出到 `integration/reports/legacy-exit/`
- `ci.ps1` 会在执行 build/test 前先生成 `integration/reports/ci/legacy-exit/`

## Simulation 受控生产测试

- [受控生产测试说明 V1](/D:/Projects/SiligenSuite/docs/validation/simulation-controlled-production-test-v1.md)
- [受控生产测试发布结论模板 V1](/D:/Projects/SiligenSuite/docs/validation/simulation-controlled-production-test-release-summary-v1.md)
- 串行执行入口：`integration/simulated-line/run_controlled_production_test.ps1`
- 单独回归入口：`integration/simulated-line/run_simulated_line.py --report-dir <report-dir>`
- Gate 校验入口：`integration/simulated-line/verify_controlled_production_gate.py --report-dir <report-dir>`
- 发布结论摘要生成入口：`integration/simulated-line/render_controlled_production_release_summary.py --report-dir <report-dir> --profile <Local|CI>`
- 默认证据位置：
  - `integration/reports/controlled-production-test/workspace-validation.json`
  - `integration/reports/controlled-production-test/simulation/simulated-line/simulated-line-summary.json`
  - `integration/reports/controlled-production-test/controlled-production-gate-summary.json`
  - `integration/reports/controlled-production-test/simulation-controlled-production-release-summary.md`
- 双轨证据：时间戳目录保留原始批次，固定目录发布最新通过结果；来源追溯见 `integration/reports/controlled-production-test/latest-source.txt`

## HIL 闭环/长稳受控测试

- [HIL 闭环/长稳发布结论模板 V1](/D:/Projects/SiligenSuite/docs/validation/hil-closed-loop-release-summary-v1.md)
- 串行执行入口：`integration/hardware-in-loop/run_hil_controlled_test.ps1`
- 单独回归入口：`integration/hardware-in-loop/run_hil_closed_loop.py --report-dir <report-dir>`
- 关键开关：`test.ps1 -IncludeHilClosedLoop`
- 默认长稳时长：`run_hil_controlled_test.ps1` 默认 `HilDurationSeconds=1800`
- 默认状态门槛：`run_hil_controlled_test.ps1` / `run_hil_closed_loop.py` 默认 `pause/resume=3`
- 参数透传（环境变量）：`SILIGEN_HIL_DISPENSER_COUNT`、`SILIGEN_HIL_DISPENSER_INTERVAL_MS`、`SILIGEN_HIL_DISPENSER_DURATION_MS`、`SILIGEN_HIL_STATE_WAIT_TIMEOUT_SECONDS`
- 报告目录规范：`workspace_validation` 会将历史 `hil-controlled-test/hil-controlled-test` 归档到 `_legacy-nested/`，并固定输出到单层 `hil-controlled-test/`
- 默认证据位置：`integration/reports/hil-controlled-test/workspace-validation.json`、`integration/reports/hil-controlled-test/hil-closed-loop-summary.json`
- 双轨证据：时间戳目录保留原始批次，固定目录发布最新通过结果；来源追溯见 `integration/reports/hil-controlled-test/latest-source.txt`

## Sim Observer 验收

- [P0 验收矩阵](/D:/Projects/SiligenSuite/docs/validation/sim-observer-p0-acceptance-v1.md)
- [P1 验收矩阵](/D:/Projects/SiligenSuite/docs/validation/sim-observer-p1-acceptance-v1.md)
- [统一验收矩阵](/D:/Projects/SiligenSuite/docs/validation/sim-observer-unified-acceptance-v1.md)
- [真实 Recording 浏览验证](/D:/Projects/SiligenSuite/docs/validation/sim-observer-real-recording-validation-v1.md)
- [发布结论](/D:/Projects/SiligenSuite/docs/validation/sim-observer-release-summary-v1.md)
