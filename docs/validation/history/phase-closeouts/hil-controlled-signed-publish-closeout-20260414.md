# HIL Controlled Signed Publish Closeout

更新时间：`2026-04-14`

## 1. 目标

把已通过的 controlled HIL rehearsal `20260414T084807Z` 收口为一次正式 signed publish，并把 fixed latest authority 从旧批次提升到本次正式批次。

本轮不做代码修复，不使用 adhoc 脚本拼装 latest，不改长期规则文档。

## 2. 执行基线

- 工作根目录：`D:\Projects\SiligenSuite`
- Git HEAD：`66cd98fa039a33a688ca50124160b0aacdc9154b`
- 当前分支：`main`
- 工作树状态：dirty；本轮未做 Git 清理，直接以当前工作区作为 formal run 基线
- formal executor：`Codex + Xike`
- formal run 前 fixed latest authority：`tests/reports/hil-controlled-test/20260414T025733Z`

正式命令：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 `
  -Profile Local `
  -UseTimestampedReportDir `
  -IncludeHilCaseMatrix `
  -PublishLatestOnPass:$true `
  -Executor "Codex + Xike"
```

## 3. 执行结果

- 本次时间戳报告目录：`tests/reports/hil-controlled-test/20260414T095201Z`
- `offline prerequisites`：`passed`，`40 passed / 0 failed / 0 known_failure / 0 skipped`
- `hardware smoke`：`passed`
- `hil-closed-loop`：`passed`
  - `dxf_case_id=rect_diag`
  - `duration_seconds=1800`
  - `iterations=599`
  - `timeout_count=0`
  - `safety_preflight_passed=true`
  - `operator_override_used=false`
- `hil-case-matrix`：`passed`
  - `mode=both`
  - `dxf_case_id=rect_diag`
  - `rounds_executed=20`
  - `passed_rounds=20`
  - `failed_rounds=0`
  - `estop_occurrences=0`
  - `limit_occurrences=0`
  - `inferred_collision_occurrences=0`
- `controlled gate`：`passed`
  - `overall_status=passed`
  - `19/19` checks passed
- `release summary`：`通过`
  - 执行人已记录为 `Codex + Xike`

## 4. Latest Authority 切换

- formal run 前：`tests/reports/hil-controlled-test/latest-source.txt` 指向 `20260414T025733Z`
- formal run 后：`tests/reports/hil-controlled-test/latest-source.txt` 已切换到 `20260414T095201Z`
- fixed latest gate：`tests/reports/hil-controlled-test/hil-controlled-gate-summary.json`
  - `generated_at=2026-04-14T18:25:28.943338+08:00`
  - `overall_status=passed`

结论：

- 本轮已完成 signed publish
- fixed latest authority 已刷新到 `20260414T095201Z`
- 后续未带非空 `-Executor` 的诊断回合不得覆盖 `tests/reports/hil-controlled-test/latest-source.txt`

## 5. 正式证据

- `tests/reports/hil-controlled-test/20260414T095201Z/offline-prereq/workspace-validation.json`
- `tests/reports/hil-controlled-test/20260414T095201Z/hardware-smoke/hardware-smoke-summary.json`
- `tests/reports/hil-controlled-test/20260414T095201Z/hil-closed-loop-summary.json`
- `tests/reports/hil-controlled-test/20260414T095201Z/hil-case-matrix/case-matrix-summary.json`
- `tests/reports/hil-controlled-test/20260414T095201Z/hil-controlled-gate-summary.json`
- `tests/reports/hil-controlled-test/20260414T095201Z/hil-controlled-release-summary.md`
- `tests/reports/hil-controlled-test/latest-source.txt`

## 6. 观察点与非目标

- 延续上一轮 handoff 的已知观察点：`arc_circle_quadrants` canonical dry-run 终态曾出现 `effective_interlocks.home_boundary_x_active=true`
- 该观察点不属于本次 formal signed publish 的 blocking 条件；本次正式入口仍按默认 `rect_diag` 受控路径执行，并未因此触发 gate / safety 异常
- 本轮 closeout 只代表 `limited-hil` 受控测试门禁与 latest authority 收口完成，不代表：
  - 工艺质量签收完成
  - 整机产能签收完成
