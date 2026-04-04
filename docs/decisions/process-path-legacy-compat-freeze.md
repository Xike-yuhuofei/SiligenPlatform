# Process-Path Legacy Compat Freeze

更新时间：`2026-04-04`

## 1. 决策结论

- `IDXFPathSourcePort` 继续保留为 `process-path` owner 提供的 legacy DXF contract，但它不是当前 live DXF 规划主链的必需入口。
- live DXF 规划主链固定为：`.pb -> IPathSourcePort::LoadFromFile -> ProcessPathFacade`。
- DXF legacy compat 实现的临时 owner 固定为 `modules/workflow/adapters/infrastructure/adapters/planning/dxf/`。
- `modules/dxf-geometry/adapters/dxf/` 这套同构实现判定为重复 legacy residue，不再保留 owner 身份。
- `modules/workflow/domain/domain/trajectory/` 判定为 orphan residue，不允许重新接入默认构建。

## 2. 当前允许保留的兼容面

- 契约层：
  - `modules/process-path/contracts/include/process_path/contracts/IPathSourcePort.h`
  - `modules/process-path/contracts/include/process_path/contracts/IDXFPathSourcePort.h`
- owner 内兼容桥：
  - `modules/process-path/domain/trajectory/ports/IPathSourcePort.h`
  - `modules/process-path/domain/trajectory/ports/IDXFPathSourcePort.h`
- 临时 compat 实现：
  - `modules/workflow/adapters/infrastructure/adapters/planning/dxf/`
- 直接验证：
  - `modules/workflow/tests/process-runtime-core/unit/infrastructure/adapters/planning/dxf/DXFAdapterFactoryTest.cpp`
  - 相关仓库级 contract test

## 2.1 已完成零引用退场

- `modules/workflow/domain/include/domain/trajectory/ports/IPathSourcePort.h`
- `modules/workflow/domain/include/domain/trajectory/ports/IDXFPathSourcePort.h`
- `modules/workflow/domain/include/domain/trajectory/domain-services/GeometryNormalizer.h`
- `modules/workflow/domain/include/domain/trajectory/domain-services/ProcessAnnotator.h`
- `modules/workflow/domain/include/domain/trajectory/domain-services/TrajectoryShaper.h`
- `modules/workflow/domain/include/domain/trajectory/value-objects/Path.h`
- `modules/workflow/domain/include/domain/trajectory/value-objects/Primitive.h`
- `modules/workflow/domain/include/domain/trajectory/value-objects/ProcessConfig.h`
- `modules/workflow/domain/include/domain/trajectory/value-objects/ProcessPath.h`
- `modules/workflow/domain/include/domain/trajectory/value-objects/GeometryUtils.h`
- `modules/workflow/domain/include/domain/trajectory/value-objects/GeometryBoostAdapter.h`
- `modules/workflow/domain/include/domain/trajectory/value-objects/PlanningReport.h`

以上 `workflow` public thin bridge 已完成仓内零引用复核并删除；默认 consumer 现已直接依赖 canonical `process_path/contracts/*`，`workflow/domain/domain/dispensing/planning/UnifiedTrajectoryPlannerService.*` residual 也已同步退场，不再反向占用该批 bridge。`process-path/domain/trajectory/value-objects/*` 仍是 owner 内部实现入口，不属于本批删除范围。

## 2.2 当前保守保留项

- `modules/process-path/domain/trajectory/ports/IPathSourcePort.h`
- `modules/process-path/domain/trajectory/ports/IDXFPathSourcePort.h`

以上对象当前仍保留为 owner 内兼容桥；`process-path/domain/trajectory/value-objects/*` 不再作为保守保留项继续存在。

## 2.2.1 owner 内部收敛与退场

- `modules/process-path/domain/trajectory/value-objects/Primitive.h`
- `modules/process-path/domain/trajectory/value-objects/Path.h`
- `modules/process-path/domain/trajectory/value-objects/ProcessConfig.h`
- `modules/process-path/domain/trajectory/value-objects/ProcessPath.h`
- `modules/process-path/domain/trajectory/value-objects/GeometryUtils.h`
- `modules/process-path/domain/trajectory/value-objects/GeometryBoostAdapter.h`

本批 owner 内部收敛结论：

- owner 内 `GeometryNormalizer`、`ProcessAnnotator`、`TrajectoryShaper`、`SplineApproximation`、`PathRegularizer` 与 `ProcessPathFacade` 已全部切换为直接消费 `process_path/contracts/*`。
- `Primitive.h`、`Path.h`、`ProcessConfig.h`、`ProcessPath.h`、`GeometryUtils.h` 不再作为 owner alias 头保留，已从 `process-path/domain/trajectory/value-objects/` 退场。
- `GeometryBoostAdapter.h` 经零引用复核后判定为 dead residue，已从 owner 内退场，不再保留“待确认实现面”身份。
- `process_path/contracts/GeometryUtils.h` 不再承载 `Siligen::Domain::Trajectory::ValueObjects::*` compat 命名空间；几何真值只保留在 `Siligen::ProcessPath::Contracts::*`。
- `ProcessPathFacade` 与 owner domain-services 现已完全共用 canonical contract 类型，不再维护 Domain/Contracts 双套 schema 转换。

## 2.3 本轮已收口 consumer

- `modules/dispense-packaging/domain/dispensing/planning/domain-services/UnifiedTrajectoryPlannerService.*`
- `modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp`
- `modules/workflow/domain/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp`

本轮 consumer 收口结论：

- `dispense-packaging` 不再直连 `process-path` owner domain target `siligen_process_path_domain_trajectory`。
- `UnifiedTrajectoryPlannerService` 已改为消费 `application/services/process_path/ProcessPathFacade.h` 与 `process_path/contracts/*`，不再 include `GeometryNormalizer.h`、`ProcessAnnotator.h`、`TrajectoryShaper.h` 等 owner domain-services bridge。
- `UnifiedTrajectoryPlannerService::Plan(...)` 已改为返回 `Result<UnifiedTrajectoryPlanResult>`，并将 `ProcessPathFacade::Build(...)` 的失败阶段与错误信息显式透传给 consumer。
- `topology_repair.enable` 在该 consumer 链路固定为 `false`，避免与现有 `DispensingPlannerService` 预处理/优化链重复生效。
- 该批改动属于 consumer 编排层收口，不代表 `process-path/domain/trajectory/value-objects/*` owner 内部旧命名空间已经进入清理阶段。

本轮新增守卫：

- `modules/dispense-packaging` 禁止重新链接 `siligen_process_path_domain_trajectory`
- `UnifiedTrajectoryPlannerService.cpp` 禁止重新 include `process-path` owner domain-services 头
- `modules/dispense-packaging` 与 `modules/workflow/domain/domain/dispensing` 禁止重新使用 `Siligen::Domain::Trajectory::ValueObjects::*`

本轮直接验证：

- `cmake --build build --config Debug --target siligen_dispense_packaging_unit_tests`
- `cmake --build build --config Debug --target siligen_unit_tests`
- `ctest --test-dir build -C Debug --output-on-failure -R "siligen_unit_tests|siligen_dispense_packaging_unit_tests|siligen_process_path_unit_tests|siligen_motion_planning_unit_tests"`
- `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/validation/assert-module-boundary-bridges.ps1 -WorkspaceRoot D:/Projects/wt-task154`

## 3. 明确禁止项

- 禁止在默认 runtime 装配链中注册或消费 `IDXFPathSourcePort`。
- 禁止 live workflow 主链绕过 `.pb + IPathSourcePort + ProcessPathFacade` 重新接入 DXF 自动预处理 compat 入口。
- 禁止新增任何模块直接复用 `AutoPathSourceAdapter`、`DXFAdapterFactory`、`DXFMigrationValidator`、`DXFMigrationManager`。
- 禁止把 `modules/workflow/domain/domain/trajectory/` 或 `modules/dxf-geometry/adapters/dxf/` 重新接回默认构建。

## 4. 存续窗口与退出条件

兼容面当前只允许用于以下场景：

- legacy 测试
- 迁移校验
- 显式设置 `SILIGEN_DXF_AUTOPATH_LEGACY=1` 的临时回退

兼容面后续退出必须同时满足：

- 默认部署链零引用 `IDXFPathSourcePort`
- 默认构建图只保留 canonical PB 读取 target，不再默认编译 DXF legacy compat 实现
- repo 内不存在第二份 DXF compat 实现
- repo 内不存在 `workflow/domain/domain/trajectory` 这类 orphan trajectory residue

## 5. 实施约束

- canonical PB 读取 target 与 legacy compat target 必须拆分。
- default consumer 只能链接 canonical PB target。
- legacy compat target 只能被显式 legacy tests 或迁移校验链接。
