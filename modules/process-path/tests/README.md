# tests

process-path 的模块级验证入口应收敛到此目录。

## 当前最小正式矩阵

- `unit/domain/trajectory/PathRegularizerTest.cpp`
  - 冻结路径规整的局部几何规则。
- `unit/application/services/process_path/ProcessPathFacadeTest.cpp`
  - 冻结 facade 对归一化、工艺标注、整形链路，以及 `TopologyRepairPolicy::Auto` metadata 准入门禁的 owner 行为。
- `contract/ProcessPathContractsTest.cpp`
  - 冻结 `ProcessPathFacade` request/result、`IPathSourcePort` 与 `ProcessPath` live contract surface。
- `golden/ProcessPathBuildGoldenTest.cpp`
  - 冻结 `lead-on/lead-off` 场景的稳定构建摘要，防止路径段标签和长度漂移。
- `golden/LeadOnPolicyGoldenTest.cpp`
  - 冻结默认 approach 不出胶、显式 priming 才允许预流的工艺摘要。
- `integration/ProcessPathToMotionPlanningIntegrationTest.cpp`
  - 覆盖 `ProcessPathFacade -> MotionPlanningFacade` 的最小跨模块闭环。
- `regression/RepairPolicyRegressionTest.cpp`
  - 冻结 topology repair policy 的主路径语义，防止再次退回旧 `enable` 裸开关或默认启用 repair。
- `regression/TopologyRepairNoOpRegressionTest.cpp`
  - 冻结简单单轮廓在 `TopologyRepairPolicy::Auto` 下不应产生误修补的 diagnostics 基线。
- `regression/TopologyRepairContourOrderingRegressionTest.cpp`
  - 冻结 contour reorder / reverse / rotate 的高风险排序语义与计数字段。
- `regression/PbFixtureRegressionTest.cpp`
  - 复用仓库内 `.pb` fixture，冻结 real-file success / repair / rejection 语义，不再依赖外部绝对路径。

## 边界

- 模块内 `tests/` 负责 `process-path` owner 级证明，不用仓库级 `tests/integration` 代替。
- 本目录的 integration 只消费 `motion-planning` public surface，不改 `M7` owner 行为。
- facade test 只保留 `ProcessPathFacade` owner 行为，不再承载 topology repair 深诊断或外部样本调试职责。
- `regression/` 负责高价值风险样本与策略矩阵；凡是需要冻结 diagnostics 细项或 repo-owned `.pb` 样本的场景，优先落在 `regression/`。
- repo-owned `.pb` 样本分为 canonical producer case 与 process-path-only regression case；后者只服务 owner 回归，不自动扩展到 preview / simulation 契约。
