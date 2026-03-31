# SiligenSuite Agent Rules

## 回应原则（最高优先级）
- 必须保持严格中立、专业和工程导向的回应风格。
- 判断只能基于事实、逻辑和技术依据，不得因用户语气而迎合、附和或改变结论。
- 禁止使用模棱两可的表述，结论必须明确且可执行。

## 语言
- 必须使用简体中文与用户沟通。

## Git 分支命名（强制）
- 所有新建分支必须使用统一格式：`<type>/<scope>/<ticket>-<short-desc>`。
- 不符合该格式的分支禁止继续开发，必须先重命名为合规名称。
- 详细规则、字段约束、示例与校验建议统一以 `docs/onboarding/git-branch-naming.md` 为准。

### 字段约束
- `type`：使用 `docs/onboarding/git-branch-naming.md` 中批准的白名单，当前包含 `feat`、`fix`、`refactor`、`docs`、`test`、`chore`、`perf`、`build`、`ci`、`revert`、`spike`。
- `scope`：使用稳定的小写模块或责任域名称，推荐优先采用 `docs/onboarding/git-branch-naming.md` 中的 project scopes。
- `ticket`：优先使用任务系统编号（如 `MC-142`、`BUG-311`、`ARCH-057`、`SPEC-021`）；若无外部任务系统，使用文档中定义的内部 ticket 前缀。
- `short-desc`：英文小写短描述，使用 kebab-case，例如 `debug-instrumentation`。

### 示例
- `chore/hmi/SS-142-debug-instrumentation`
- `fix/runtime/SS-205-startup-timeout`
- `chore/hmi/TASK-144-debug-instrumentation`
- `build/infra/TASK-122-upgrade-clang-cl`
- `ci/build/TASK-133-add-static-analysis`

## Active Technologies
- Markdown docs; PowerShell 7; Python 3.11; C++17/CMake 3.20+ + Root `build.ps1` / `test.ps1` / `ci.ps1`; CMake workspace targets; `application-contracts`; `engineering-contracts`; `engineering-data`; `test-kit`; protobuf; spdlog (refactor/arch/NOISSUE-architecture-refactor-spec)
- Git-tracked Markdown/spec files plus repository filesystem assets under canonical roots; no database introduced by this feature (refactor/arch/NOISSUE-architecture-refactor-spec)
- Markdown docs; PowerShell 7; Python 3.11; C++17 / CMake 3.20+ + Root entry points (`build.ps1`, `test.ps1`, `ci.ps1`); `scripts/migration/validate_workspace_layout.py`; `scripts/migration/legacy-exit-checks.py`; `scripts/validation/run-local-validation-gate.ps1`; module `CMakeLists.txt` + `module.yaml`; `shared/testing/test-kit`; protobuf; spdlog (refactor/arch/NOISSUE-dsp-e2e-full-migration)
- Git-tracked repository filesystem assets under canonical roots plus validation reports under `tests/reports/` (refactor/arch/NOISSUE-dsp-e2e-full-migration)
- Python 3.11（`apps/hmi-app`、测试与脚本）、C++17 / CMake 3.20+（`apps/planner-cli`、`apps/runtime-gateway`、`modules/workflow`）、PowerShell 7（根级入口与 smoke） + PyQt5 / PyQtWebEngine；`apps/planner-cli` DXF 命令面；`modules/workflow` 点胶规划/预览用例；`shared/contracts/application/commands/dxf.command-set.json`；`pytest` 与根级验证脚本 (test/hmi/TASK-001-offline-dxf-preview-consistency)
- Git 跟踪的 DXF/契约/基线资产（`samples/`、`shared/contracts/`、`tests/baselines/`）以及验证输出目录 `tests/reports/`；无数据库 (test/hmi/TASK-001-offline-dxf-preview-consistency)
- Python 3.11（`apps/hmi-app`、测试与脚本）、C++17 / CMake 3.20+（`apps/runtime-gateway`、`modules/workflow`）、PowerShell 7（根级入口与 smoke） + PyQt5 / PyQtWebEngine；`apps/runtime-gateway` 在线命令链；`modules/workflow` 点胶规划/预览用例；`shared/contracts/application/commands/dxf.command-set.json`；`pytest` 与根级验证脚本 (test/hmi/TASK-001-offline-dxf-preview-consistency)
- C++17 / CMake 3.20+（`modules/dispense-packaging`、`modules/workflow`、`apps/runtime-gateway`）、Python 3.11（`apps/hmi-app` 与 Python 协议/GUI 测试）、PowerShell 7（根级 `build.ps1` / `test.ps1` / `ci.ps1` 与 workflow 脚本） + `modules/dispense-packaging` `DispensePlanningFacade` / `PreviewSnapshotService` / `TriggerPlanner`；`modules/workflow` `PlanningUseCase` / `DispensingWorkflowUseCase` / `DispensingPlannerService`；`apps/runtime-gateway` TCP `dxf.preview.snapshot` dispatcher；`apps/hmi-app` preview session / `main_window.py`；`shared/contracts/application/commands/dxf.command-set.json`；GoogleTest / CTest；`pytest` (fix/workflow/SPEC-001-glue-point-alignment)
- Git 跟踪的仓库源码、spec 产物、契约文档、测试与基线资产；无数据库 (fix/workflow/SPEC-001-glue-point-alignment)
- C++17 / CMake 3.20+（`modules/dispense-packaging`、`modules/workflow`、`apps/runtime-gateway`）、Python 3.11（`apps/hmi-app` 测试与 mock 工具）、PowerShell 7（根级 `build.ps1` / `test.ps1` / `ci.ps1` 与 spec-kit 脚本） + `modules/dispense-packaging` `DispensePlanningFacade`、`DispensingPlannerService`、`TriggerPlanner`、`PreviewSnapshotService`、`TrajectoryTriggerUtils`；计划新增 `AuthorityTriggerLayoutPlanner`、`PathArcLengthLocator`、`CurveFlatteningService`、`AuthorityTriggerLayout`；`modules/workflow` `PlanningUseCase`、`DispensingWorkflowUseCase`、`WorkflowPreviewSnapshotService`；`apps/runtime-gateway` `TcpCommandDispatcher`；`apps/hmi-app` `main_window.py`；`shared/contracts/application/commands/dxf.command-set.json` 与 fixtures；GoogleTest / CTest；`pytest` (fix/workflow/SPEC-002-global-glue-spacing)
- Git 跟踪的仓库源码、spec 产物、协议契约、fixture 与测试资产；无数据库 (fix/workflow/SPEC-002-global-glue-spacing)

## Recent Changes
- refactor/arch/NOISSUE-architecture-refactor-spec: Added Markdown docs; PowerShell 7; Python 3.11; C++17/CMake 3.20+ + Root `build.ps1` / `test.ps1` / `ci.ps1`; CMake workspace targets; `application-contracts`; `engineering-contracts`; `engineering-data`; `test-kit`; protobuf; spdlog
