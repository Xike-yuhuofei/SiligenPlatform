# Feature Specification: ARCH-201 Stage B - 边界收敛与 US2 最小闭环

**Feature Branch**: `refactor/motion/ARCH-201-m7-owner-boundary-repair`
**Created**: 2026-03-27
**Status**: In Progress
**Input**: User description: "先把当前混合工作树收敛回 ARCH-201 合法范围，再完成 US2 的最小闭环，让 M7 只保留规划职责，M9 承接 runtime/control"

## Stage Scope

- **目标**: 先完成当前工作树的 Stage B 范围收敛，再把 runtime/control owner seam 固定到 `modules/runtime-execution`。
- **代码白名单**: `modules/motion-planning`、`modules/process-path`、`modules/runtime-execution`、`modules/workflow`、`scripts/validation`。
- **compile-only collateral**: `shared/contracts/device`、`apps/runtime-gateway`。
- **本阶段不包含**: US3 全量 public surface/骨架一致化收口；US4 CMP 最终收口；白名单外目录的恢复或继续实现。
- **canonical 落点**:
  - `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/`
  - `modules/runtime-execution/application/include/runtime_execution/application/usecases/motion/`
  - `modules/runtime-execution/runtime/host/runtime/motion/`

## User Scenarios & Testing *(mandatory)*

### User Story 1 - 范围收敛与 canonical 对齐 (Priority: P1)

作为架构维护者，我需要先把当前 Stage B 的活动范围收敛回白名单目录，并让 `spec.md`、`plan.md`、`tasks.md`、门禁脚本和说明文档统一指向真实 canonical 路径，避免后续验证、提交和复审继续基于过期目录假设。

**Why this priority**: 如果范围和文档口径不收敛，US2 的最小闭环即使代码已落地，也会继续被旧任务、旧路径和错误门禁掩盖。
**Independent Test**: 在仅完成本故事的情况下，活动工作树只保留 Stage B 白名单目录改动，且文档/门禁不再引用 `runtime/host/services/motion` 作为 M9 主 owner 路径。

**Acceptance Scenarios**:

1. **Given** 当前工作树混杂多个阶段的未提交改动，**When** 形成 keep/park 边界，**Then** Stage B 仅保留白名单目录内改动进入实现与收尾。
2. **Given** Stage B 已选择 `runtime-execution/runtime/host/runtime/motion` 作为 M9 canonical 落点，**When** 更新 spec/plan/tasks 与 quickstart，**Then** 旧 `runtime/host/services/motion` 路径不再作为 owner 计划项出现。

---

### User Story 2 - M9 runtime/control 最小闭环 (Priority: P1)

作为运行时维护者，我需要让 homing、ready-zero、jog、control/status/IO 的主 owner 入口迁移到 `modules/runtime-execution`，同时保留最小兼容 seam，使 `M7 motion-planning` 回归规划职责，`apps/runtime-gateway` 与 host/container 可以通过单一入口完成最小闭环。

**Why this priority**: 这是 Stage B 的核心交付；如果 runtime/control owner 仍停留在 M7 或 workflow 内部散点，后续 US3/US4 无法建立稳定边界。
**Independent Test**: 在仅完成本故事的情况下，`NoRuntimeControlLeakTest` 与 `MotionControlMigrationTest` 可以说明旧 M7/workflow 公开头已退化为 shim，且 gateway/host 已通过 `MotionControlUseCase` 接入 M9 runtime/control surface。

**Acceptance Scenarios**:

1. **Given** 旧 `domain/motion/ports/*.h` 与 `domain-services/*Impl.h` 仍被下游 include，**When** 完成迁移，**Then** 这些头文件只保留 shim/alias 到 `runtime-execution` owner，不再声明新的 runtime/control owner 类型。
2. **Given** runtime host 需要从 motion runtime port 组装控制/状态服务，**When** 完成迁移，**Then** `runtime-execution/runtime/host/runtime/motion/WorkflowMotionRuntimeServicesProvider.*` 承接 host provider 装配。
3. **Given** `apps/runtime-gateway` 需要维持既有外部协议行为，**When** 完成迁移，**Then** builder/facade 只重接到 `MotionControlUseCase`，不新增 TCP JSON 字段、不新增命令、不改变对外行为。
4. **Given** `MotionPlanningFacade` 已作为 M7 规划入口，**When** Stage B 完成，**Then** `MotionTrajectory` 的规划主输出语义保持不变。

---

### Edge Cases

- 旧 include 路径仍被其他模块引用时，只允许保留 shim/alias；不得恢复 owner 实现。
- `workflow` 在本阶段允许保留 thin compatibility shell，但不得继续吸收新的 runtime/control 主实现。
- 若白名单 park 导致顶层 superbuild 失配，证据文档必须记录具体阻塞目录和日期，不能伪报验证通过。
- `shared/contracts/device` 仅允许做 compile-only contract 补位；不得借本阶段扩展设备语义。

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Stage B 活动工作树 MUST 收敛到白名单目录与 compile-only collateral。
- **FR-002**: `spec.md`、`plan.md`、`tasks.md`、`quickstart.md` MUST 使用 `runtime-execution/runtime/host/runtime/motion` 作为 M9 host motion canonical 落点。
- **FR-003**: `modules/runtime-execution/contracts/runtime` MUST owner `IMotionRuntimePort` 与 `IIOControlPort`。
- **FR-004**: `modules/runtime-execution/application/usecases/motion` MUST owner `MotionControlUseCase`，作为 homing、ready-zero、jog、control/status/IO 的最小统一入口。
- **FR-005**: `modules/runtime-execution/runtime/host/runtime/motion` MUST 提供 host 侧 provider/orchestrator 装配点。
- **FR-006**: `modules/motion-planning/domain/motion/*` 与 `modules/workflow/domain/include/domain/motion/*` MUST NOT 再声明 runtime/control owner 端口或服务实现；只允许 shim/alias。
- **FR-007**: `modules/process-path` 与 `modules/workflow` 在本阶段 MUST 仅保留路径事实或 thin compatibility shell，不得重新吸收 M9 owner 语义。
- **FR-008**: `apps/runtime-gateway` MUST 只做 include、构造注入与 facade builder 重接，不得改变 TCP JSON 协议字段或外部行为。
- **FR-009**: `scripts/validation/assert-module-boundary-bridges.ps1` MUST 覆盖 M7 -> M9 runtime/control owner 迁移后的 required/forbidden 规则。
- **FR-010**: `NoRuntimeControlLeakTest`、`MotionControlMigrationTest`、US2 evidence、quickstart、traceability MUST 与 Stage B 实际落地保持一致。

### Key Entities *(include if feature involves data)*

- **MotionPlanningOwnerSurface**: `M7 motion-planning` 暴露的规划事实与求解入口，核心是 `MotionPlanner`、`MotionTrajectory`、`MotionPlanningReport`。
- **MotionRuntimeOwnerSurface**: `M9 runtime-execution` 暴露的 runtime/control owner surface，核心是 `IMotionRuntimePort`、`IIOControlPort`、`MotionControlUseCase`。
- **CompatibilityShim**: 旧 `motion-planning` / `workflow` 公开头上的短期 alias/forwarding seam，用于维持编译兼容但不再承载 owner 语义。
- **BoundaryComplianceEvidence**: Stage B 的门禁报告、测试入口、quickstart、traceability 与 US2 证据文档。

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Stage B 文档、任务与 quickstart 不再把 `runtime/host/services/motion` 记作 M9 主 owner 路径。
- **SC-002**: `modules/motion-planning` 与 `modules/workflow/domain/include` 下的 runtime/control 公开头均退化为 shim/alias。
- **SC-003**: `apps/runtime-gateway` 与 runtime host/container 的 motion 接入统一收敛到 `MotionControlUseCase` 与 `runtime/host/runtime/motion` provider seam。
- **SC-004**: `NoRuntimeControlLeakTest` 与 `MotionControlMigrationTest` 已加入 canonical CMake 测试入口。
- **SC-005**: Stage B 证据文档能明确说明已完成项、未完成项和被工作树前置状态阻断的验证项，并给出具体阻断路径。
