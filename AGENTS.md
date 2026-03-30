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

## Codex 技能工作流（项目内）

项目内技能入口：`.agents/skills/`

推荐按以下顺序使用：

1. `ss-scope-clarify`：需求澄清与范围锁定
2. `ss-arch-plan`：架构与测试计划
3. `ss-premerge-review`：预合并代码审查
4. `ss-mock-regression`：无机台 / mock 批量功能回归
5. `ss-feature-deep-test`：单功能无机台深挖测试
6. `ss-qa-regression`：测试-修复-回归
7. `ss-release-pr`：发版与 PR 提交
8. `ss-doc-sync`：发布后文档同步
9. `ss-weekly-retro`：阶段性复盘

按需技能：

- `ss-ui-plan-check`：UI/交互方案审查（HMI 相关需求）
- `ss-incident-debug`：故障调查与根因分析
- `ss-safety-guard`：高风险操作安全护栏
- `ss-mock-regression`：当用户要求对多个功能执行无机台 / mock 回归并直接跑到结论时使用
- `ss-feature-deep-test`：当用户要求专门深挖某一个功能的 HMI、协议、mock、领域规则链路时使用

命名优化原则：

- 统一前缀 `ss-`（SiligenSuite）
- 统一“动词-对象”结构，减少语义歧义
- 名称直接对应交付阶段，便于团队记忆与检索

执行约束补充：

1. 运行技能前，先执行 `scripts/validation/resolve-workflow-context.ps1` 统一 `ticket/branchSafe/timestamp`
2. 高风险命令必须通过 `scripts/validation/invoke-guarded-command.ps1` 执行

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
