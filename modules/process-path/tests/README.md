# tests

process-path 的模块级验证入口应收敛到此目录。

## 当前最小正式矩阵

- `unit/domain/trajectory/PathRegularizerTest.cpp`
  - 冻结路径规整的局部几何规则。
- `unit/application/services/process_path/ProcessPathFacadeTest.cpp`
  - 冻结 facade 对归一化、工艺标注和整形链路的最小 owner 行为。
- `contract/ProcessPathContractsTest.cpp`
  - 冻结 `ProcessPathFacade` request/result、`IPathSourcePort` 与 `ProcessPath` live contract surface。
- `golden/ProcessPathBuildGoldenTest.cpp`
  - 冻结 `lead-on/lead-off` 场景的稳定构建摘要，防止路径段标签和长度漂移。
- `integration/ProcessPathToMotionPlanningIntegrationTest.cpp`
  - 覆盖 `ProcessPathFacade -> MotionPlanningFacade` 的最小跨模块闭环。

## 边界

- 模块内 `tests/` 负责 `process-path` owner 级证明，不用仓库级 `tests/integration` 代替。
- 本目录的 integration 只消费 `motion-planning` public surface，不改 `M7` owner 行为。
