# Process-Path Legacy Compat Closeout

更新时间：`2026-04-09`

## 1. 决策结论

- `modules/process-path/contracts/include/process_path/contracts/IPathSourcePort.h` 保留为唯一 live 输入 port contract。
- `IPathSourcePort` 的 live 语义命名空间固定为 `Siligen::ProcessPath::Contracts`；`Siligen::Domain::Trajectory::Ports` 不再作为 public seam 暴露。
- live 输入主链固定为：`.pb -> IPathSourcePort::LoadFromFile -> ProcessPathFacade`。
- `modules/process-path/contracts/include/process_path/contracts/IDXFPathSourcePort.h` 已删除；它不再属于 `process-path` public surface。
- `modules/process-path/domain/trajectory/ports/IPathSourcePort.h` 与 `modules/process-path/domain/trajectory/ports/IDXFPathSourcePort.h` 已删除；owner bridge 头不再参与 live build。

## 2. 最终处置结果

- `migrated`
  - `modules/process-path/contracts/include/process_path/contracts/IPathSourcePort.h`
  - 处置：从旧 `Siligen::Domain::Trajectory::Ports` seam 迁移到 `Siligen::ProcessPath::Contracts`。
- `deleted`
  - `modules/process-path/contracts/include/process_path/contracts/IDXFPathSourcePort.h`
  - `modules/process-path/domain/trajectory/ports/IPathSourcePort.h`
  - `modules/process-path/domain/trajectory/ports/IDXFPathSourcePort.h`

## 3. consumer 收口证据

- `apps/runtime-service/bootstrap/ContainerBootstrap.cpp`
- `apps/runtime-service/container/ApplicationContainer.Dispensing.cpp`
- `modules/workflow/application/include/application/planning-trigger/PlanningUseCase.h`
- `modules/workflow/application/planning-trigger/PlanningUseCase.cpp`
- `modules/dxf-geometry/adapters/include/dxf_geometry/adapters/planning/dxf/PbPathSourceAdapter.h`
- `modules/dxf-geometry/adapters/dxf/PbPathSourceAdapter.cpp`
- `modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.h`
- `modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp`
- `modules/dispense-packaging/domain/dispensing/planning/domain-services/ContourOptimizationService.h`
- `modules/dispense-packaging/domain/dispensing/planning/domain-services/ContourOptimizationService.cpp`
- `modules/motion-planning/tests/unit/domain/motion/MainlineTrajectoryAuditTest.cpp`

以上 live consumer / direct regression consumer 已统一切换到 `Siligen::ProcessPath::Contracts::*`。

## 4. 验证焦点

- `process-path` public headers 不再暴露 `Siligen::Domain::Trajectory::Ports`。
- repo live source 不再 include `domain/trajectory/ports/*.h` bridge。
- `process-path` public surface 不再保留 `IDXFPathSourcePort` legacy DXF contract。

## 5. 禁止回退

- 禁止恢复 `modules/process-path/domain/trajectory/ports/*.h` bridge 头。
- 禁止恢复 `modules/process-path/contracts/include/process_path/contracts/IDXFPathSourcePort.h`。
- 禁止在 live source 中重新使用 `Siligen::Domain::Trajectory::Ports::*` 作为 `process-path` contract seam。
