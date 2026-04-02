# Phase 0 Research: Layered Test System

## Decision 1: 继续以根级 `build.ps1` / `test.ps1` / `ci.ps1` 作为唯一正式执行面

- Decision: 分层测试体系不引入新的默认 runner、第二套 CI 编排或平行测试根。所有正式验证都继续通过根级 `build.ps1`、`test.ps1`、`ci.ps1` 与 `scripts/validation/run-local-validation-gate.ps1` 触发，`python -m test_kit.workspace_validation` 仍由这些入口统一编排。
- Rationale: `docs/architecture/build-and-test.md` 已冻结这四个入口为正式根级执行面，`test.ps1` 和 `ci.ps1` 也已经把 suite 与 evidence 输出收敛到 canonical 路径。若再增加新的默认 runner，只会制造第二真值和门禁分叉。
- Alternatives considered:
  - 为分层体系新增独立 `layered-test.ps1` 作为主入口: 拒绝。会破坏根级入口唯一性。
  - 让各模块脚本直接承担默认门禁职责: 拒绝。无法统一 evidence、suite 与 CI 行为。

## Decision 2: 验证分层直接映射到仓库现有承载面，而不是重新命名整套目录

- Decision: 分层语义基于现有仓库承载面冻结为七层：`L0 structure-gate`、`L1 module-contract`、`L2 offline-integration`、`L3 simulated-e2e`、`L4 performance`、`L5 limited-hil`、`L6 closeout`。这些层分别落在 `scripts/migration/` 与 `scripts/validation/`、模块 `*/tests`、`tests/integration/`、`tests/e2e/simulated-line/`、`tests/performance/`、`tests/e2e/hardware-in-loop/` 与 `tests/reports/` / closeout 文档。
- Rationale: `tests/CMakeLists.txt`、`tests/integration/README.md`、`tests/e2e/README.md`、`tests/performance/README.md` 已经把仓库级验证面稳定下来。直接在现有承载面之上定义层级，既能兼容现有资产，又不需要目录迁移。
- Alternatives considered:
  - 只保留 fast/full 两层: 拒绝。无法表达 offline integration、performance、HIL 的不同成本与证据责任。
  - 把大多数覆盖都放到 GUI 或 HIL: 拒绝。与“先离线、后联机”的仓库原则冲突。

## Decision 3: 共享测试资产与夹具统一收敛到 `samples/`、`tests/baselines/`、`shared/testing/test-kit`

- Decision: 输入样本、golden baseline、共用 fixture、断言与 workspace validation 支撑分别以 `samples/`、`tests/baselines/`、`shared/testing/test-kit/` 为 canonical roots；模块测试与仓库级场景只能引用这些共享事实源，不再各自维护第二套 canonical 资产。
- Rationale: `tests/integration/README.md` 已明确 `samples/dxf/` 和 `samples/golden/` 是静态工程事实样本来源；`shared/testing/README.md` 已冻结 `test-kit` 为跨模块测试支撑根。把事实资产和共用夹具集中，才能让分层体系具备可复用性和可比较性。
- Alternatives considered:
  - 允许每个模块在私有目录维护自己的 canonical sample 和 fixture: 拒绝。会导致样本重复、口径漂移和维护成本上升。
  - 只保留 synthetic 数据，不沉淀真实样本与 baseline: 拒绝。无法覆盖工业自动化场景中的真实链路风险。

## Decision 4: 结构化证据统一以 `tests/reports/` 和必要 trace 字段为中心

- Decision: 所有层级的通过、失败、阻断和延后结论都必须回收到 `tests/reports/` 及其子目录，并使用统一 evidence bundle 口径。证据至少要能表达 `stage_id`、`artifact_id`、`module_id`、`workflow_state`、`execution_state`、`event_name`、`failure_code`、`evidence_path`。
- Rationale: `docs/architecture/build-and-test.md` 与 `docs/validation/README.md` 已经冻结了报告出口和必要回链字段。没有统一 evidence，就无法把 quick gate、CI、nightly、HIL 的结论放在同一套 closeout 语义里解释。
- Alternatives considered:
  - 只依赖控制台输出或测试框架默认日志: 拒绝。不可审计，也无法支撑 closeout。
  - 为不同 suite 使用互不兼容的报告目录和字段: 拒绝。会让失败分类与追溯失效。

## Decision 5: 运行通道按成本与风险冻结为 quick gate、full gate、nightly/perf、limited HIL

- Decision: 分层体系的调度不再采用“一把梭”策略，而是冻结为四条执行通道：`quick-gate`、`full-offline-gate`、`nightly-performance`、`limited-hil`。这些通道共用一套 layer、asset、fixture 和 evidence 语义，只是覆盖范围与触发门槛不同。
- Rationale: `test.ps1` 已按 `contracts`、`e2e`、`protocol-compatibility`、`performance` 等 suite 分层；`ci.ps1` 已把 build/test/local gate 聚合到单次执行中；`tests/performance/README.md` 已提供单独的性能画像脚本；`docs/validation/README.md` 也已把 HIL 设计为显式 opt-in。设计只需要把这些现有能力收束成明确 lane。
- Alternatives considered:
  - PR、nightly、release 都运行同一套全量矩阵: 拒绝。反馈过慢，且会过早引入高成本验证。
  - 真实硬件优先于离线 / 模拟: 拒绝。与仓库 Gate 4 和联机专项规则冲突。

## Decision 6: owner 边界保持不变，模块契约与仓库级链路验证分层承载

- Decision: 业务 owner 仍留在各自模块或应用层。模块语义输出、错误分类和状态机契约优先落在各自 `*/tests` 下；跨模块链路、模拟全链路、性能与 HIL 继续由 `tests/` 仓库级承载面统一组织；`shared/testing/` 只承载跨模块复用支撑，不承载业务语义 owner。
- Rationale: `tests/CMakeLists.txt` 已经把 canonical module test roots 与 repo-level surfaces 明确拆开；`shared/testing/README.md` 也明确禁止在共用测试根堆积业务 owner 实现。保持这一边界，才能避免“为了测试方便把 owner 搬错层”。
- Alternatives considered:
  - 把所有测试都搬到仓库级 `tests/`: 拒绝。会稀释模块 owner 责任。
  - 把共用 fixture 与路由逻辑堆到业务模块里: 拒绝。会让 `shared/testing/` 失去 canonical 共用支撑作用。
