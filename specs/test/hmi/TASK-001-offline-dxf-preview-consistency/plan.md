# Implementation Plan: HMI 在线点胶轨迹预览一致性验证

**Branch**: `test/hmi/TASK-001-offline-dxf-preview-consistency` | **Date**: 2026-03-26 | **Spec**: `D:\Projects\SiligenSuite\specs\test\hmi\TASK-001-offline-dxf-preview-consistency\spec.md`
**Input**: Feature specification from `D:\Projects\SiligenSuite\specs\test\hmi\TASK-001-offline-dxf-preview-consistency\spec.md`

## Summary

围绕“机台已就绪的在线预览”冻结一条可验收的真实链路：HMI 必须在 `online` 模式且 Supervisor 达到 `online_ready` 后，基于真实 DXF 触发在线预览，并将其与上位机准备下发到运动控制卡的数据按业务语义对齐。权威预览来源复用现有 `dxf.artifact.create -> dxf.plan.prepare -> dxf.preview.snapshot` 主链，正式成功语义固定为 `planned_glue_snapshot + glue_points`；验证闭环采用在线 GUI smoke、协议契约校验、真实 DXF 几何回归和 HIL 快照脚本。

## Technical Context

**Language/Version**: Python 3.11（`apps/hmi-app`、测试与脚本）、C++17 / CMake 3.20+（`apps/runtime-gateway`、`modules/workflow`）、PowerShell 7（根级入口与 smoke）
**Primary Dependencies**: PyQt5 / PyQtWebEngine；`apps/runtime-gateway` 在线命令链；`modules/workflow` 点胶规划/预览用例；`shared/contracts/application/commands/dxf.command-set.json`；`pytest` 与根级验证脚本
**Storage**: Git 跟踪的 DXF/契约/基线资产（`samples/`、`shared/contracts/`、`tests/baselines/`）以及验证输出目录 `tests/reports/`；无数据库
**Testing**: 根级 `.\build.ps1`、`.\test.ps1`、`.\ci.ps1`；`apps/hmi-app\scripts\online-smoke.ps1`；`pytest` 用例位于 `tests/contracts`、`tests/e2e/first-layer`、`tests/e2e/hardware-in-loop`
**Target Platform**: Windows 桌面工作站；HMI 以在线模式启动；机台、控制链和必要安全前提已准备就绪
**Project Type**: 桌面 HMI + runtime/gateway 在线预览链 + 契约与验证资产
**Performance Goals**: canonical DXF 的在线预览判断流程在 3 分钟内完成；预览生成保持在现有 HIL / first-layer 测试超时窗口内；几何基线在重复运行中可复现
**Constraints**: 真实验收仅接受真实 DXF 输入；HMI 必须在 `online_ready` 前提下生成预览；权威预览来源必须是 `planned_glue_snapshot + glue_points`；旧版 `runtime_snapshot`、`mock_synthetic`、历史残留或不可追溯结果不得进入通过结论；预览与控制卡下发准备数据按业务语义比对而非字节格式比对；仅使用 canonical workspace roots
**Scale/Scope**: 一个 feature slice，覆盖 `apps/hmi-app` 在线门禁、runtime/gateway DXF 预览契约、`modules/workflow` 执行准备语义对齐、`samples/dxf/rect_diag.dxf` 强制基线，以及相关契约/回归/证据资产

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] Branch name is compliant with `<type>/<scope>/<ticket>-<short-desc>`
- [x] Planned paths use canonical workspace roots; no new legacy default paths
- [x] Build/test validation approach is explicit via root entry points (`.\build.ps1`, `.\test.ps1`, `.\ci.ps1`) plus justified targeted checks (`apps/hmi-app\scripts\online-smoke.ps1`, `pytest tests/contracts/test_protocol_compatibility.py -q`, `pytest tests/e2e/first-layer/test_real_preview_snapshot_geometry.py -q`, `python tests/e2e/hardware-in-loop/run_real_dxf_preview_snapshot.py --config-mode real`)
- [x] Compatibility/fallback behavior is explicit and time-bounded if legacy paths are involved (`mock_synthetic` 继续存在但只允许联调/失败提示，不得进入真实验收结论；不新增默认 legacy path 或隐式 fallback）

## Project Structure

### Documentation (this feature)

```text
D:\Projects\SiligenSuite\specs\test\hmi\TASK-001-offline-dxf-preview-consistency\
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts\
└── tasks.md
```

### Source Code (repository root)

```text
D:\Projects\SiligenSuite\apps\hmi-app\
├── run.ps1
├── scripts\online-smoke.ps1
├── docs\smoke-exit-codes.md
└── src\hmi_client\

D:\Projects\SiligenSuite\apps\runtime-gateway\
└── transport-gateway\

D:\Projects\SiligenSuite\modules\workflow\
├── application\usecases\dispensing\
└── tests\process-runtime-core\unit\dispensing\

D:\Projects\SiligenSuite\shared\contracts\application\commands\dxf.command-set.json
D:\Projects\SiligenSuite\tests\contracts\test_protocol_compatibility.py
D:\Projects\SiligenSuite\tests\e2e\first-layer\test_real_preview_snapshot_geometry.py
D:\Projects\SiligenSuite\tests\e2e\hardware-in-loop\run_real_dxf_preview_snapshot.py
D:\Projects\SiligenSuite\tests\baselines\preview\rect_diag.preview-snapshot-baseline.json
D:\Projects\SiligenSuite\samples\dxf\rect_diag.dxf
D:\Projects\SiligenSuite\docs\validation\dispense-trajectory-preview-real-acceptance-checklist.md
```

**Structure Decision**: 保持现有多应用/多模块结构，不新增新的顶层项目。HMI 在线交互继续留在 `apps/hmi-app`；权威在线预览链路继续由 `apps/runtime-gateway` 与 `modules/workflow` 承接；传输层 schema 继续以 `shared/contracts/application` 为正式契约源；验证与证据资产继续落在 `tests/` 与 `docs/validation/`。

## Post-Design Constitution Re-check

- [x] 设计仍只使用 `apps/`、`modules/`、`shared/`、`tests/`、`docs/`、`samples/`、`scripts/`、`config/` canonical roots
- [x] 在线权威预览链路未把 `packages/`、`integration/`、`tools/`、`examples/` 引回为默认 owner surface
- [x] 验证计划仍显式走根级入口，并补充 online smoke / 协议契约 / 几何回归 / HIL 快照的 justified subset
- [x] 兼容行为被限定为已存在的 `mock_synthetic` 联调分支，且继续被视为失败边界，不构成真实签收路径

## Complexity Tracking

当前无宪章豁免项；本节保持空白。
