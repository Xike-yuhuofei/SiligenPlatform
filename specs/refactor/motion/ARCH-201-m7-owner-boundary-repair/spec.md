# Feature Specification: M7 MotionPlan Owner Boundary Repair

**Feature Branch**: `refactor/motion/ARCH-201-m7-owner-boundary-repair`  
**Created**: 2026-03-27  
**Status**: Draft  
**Input**: User description: "基于 modules/motion-planning 审查报告执行结构性修复，先 owner 收口，再 runtime/control 剥离，最后 public surface 与目录骨架一致化"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - M7 Owner 事实收口 (Priority: P1)

作为架构维护者，我需要 `MotionPlanner` 的源码归属、构建归属、调用归属都收敛在 `modules/motion-planning`，避免 `motion-planning` 直接编译 `process-path` 的实现文件。

**Why this priority**: 这是当前最高风险问题（P0），不解决无法开展后续边界治理。  
**Independent Test**: 在仅完成 US1 的情况下，`siligen_motion` 不再编译 `modules/process-path/domain/trajectory/domain-services/MotionPlanner.cpp`，并且 `MotionPlanningFacade` 仍可产出 `MotionTrajectory`。  

**Acceptance Scenarios**:

1. **Given** 当前 M7 直接编译 M6 的 `MotionPlanner.cpp`，**When** 完成 owner 收口改造，**Then** `modules/motion-planning/domain/motion/CMakeLists.txt` 仅编译 M7 自有实现。  
2. **Given** 应用层通过 `MotionPlanningFacade` 调用规划，**When** 切换到 M7 自有 `MotionPlanner`，**Then** 现有规划路径调用链保持可编译且行为不回归。  

---

### User Story 2 - Runtime/Control 语义剥离到 M9 (Priority: P2)

作为运行时维护者，我需要把回零、点动、控制、状态、IO 等执行语义从 M7 剥离到 `runtime-execution` owner 边界，确保 M7 仅产出规划事实。

**Why this priority**: 这是第二个 P0 风险，决定 M7/M9 的长期可维护边界。  
**Independent Test**: 在仅完成 US2 的情况下，运行时控制入口位于 `modules/runtime-execution`，M7 不再持有 runtime/control 主端口抽象。  

**Acceptance Scenarios**:

1. **Given** M7 内存在 `HomingProcess`、`JogController`、`MotionControlService` 等，**When** 完成迁移，**Then** 这些控制流程归位到 M9 并由 M9 application/runtime host 消费。  
2. **Given** 运行时执行模块负责消费 M4-M8 结果，**When** 改造完成，**Then** M7 只提供规划结果，不承担设备控制职责。  

---

### User Story 3 - Public Surface 与目录骨架一致化 (Priority: P3)

作为模块集成者，我需要 `contracts/application/domain/adapters/tests` 与真实 owner 实现一致，避免“目录完整但核心散落/转发壳”的误导。

**Why this priority**: 这是 P1 问题，影响认知成本、构建治理和后续迁移效率。  
**Independent Test**: 在仅完成 US3 的情况下，M7 应用入口成为稳定边界，空壳目录不再承载虚假职责描述。  

**Acceptance Scenarios**:

1. **Given** 当前 `services/`、`adapters/` 主要是说明文本，**When** 完成结构收敛，**Then** 文档、CMake 和真实实现职责保持一致。  
2. **Given** 上层消费者依赖 M7 public surface，**When** 完成收敛，**Then** 依赖入口稳定且不再跨模块抓取内部实现。  

---

### User Story 4 - CMP/触发边界显式化 (Priority: P3)

作为工艺链路维护者，我需要明确 M7 输出的 CMP 规划事实与 M9 执行触发包装边界，避免语义继续漂移。

**Why this priority**: 这是 P2 风险，当前尚可控，但若不治理会扩大后续拆分成本。  
**Independent Test**: 在仅完成 US4 的情况下，CMP 输入输出契约明确，执行侧触发包装逻辑不再在 M7 膨胀。  

**Acceptance Scenarios**:

1. **Given** M7 内处理 `cmp_channel`、`pulse_width_us` 与触发时间线，**When** 完成边界整理，**Then** M7 仅保留规划必要语义，执行包装归位 M9。  
2. **Given** 需要对外说明规划输出边界，**When** 更新 contracts 文档，**Then** 审查者可直接判定是否越界。  

---

### Edge Cases

- 旧 include 路径仍被其他模块引用时，兼容层必须可追踪且有退出条件。  
- 若迁移期间出现 M6/M7 或 M7/M9 双向 include，必须阻断合入。  
- 在无真实硬件依赖的本地/CI 环境下，验证门禁必须可执行并可复现。  
- 并行开发分支可能继续引用旧路径，必须提供清晰的迁移告警。  

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: `MotionPlanner` 实现 MUST 归位到 `modules/motion-planning` owner 根。  
- **FR-002**: `modules/motion-planning/domain/motion/CMakeLists.txt` MUST NOT 直接编译 `modules/process-path` 的 `.cpp`。  
- **FR-003**: `modules/process-path/domain/trajectory/CMakeLists.txt` MUST NOT 反向暴露 `modules/motion-planning` 作为 PUBLIC include 根。  
- **FR-004**: `MotionPlanningFacade` MUST 仅依赖 M7 自有规划实现或 M7 稳定契约。  
- **FR-005**: 回零/点动/控制/状态/IO 语义 MUST 从 M7 剥离到 M9 owner 范围。  
- **FR-006**: M7 contracts MUST 明确禁止 runtime/control 语义驻留。  
- **FR-007**: 边界门禁脚本 MUST 能检测 M6<->M7 和 M7<->M9 违规依赖。  
- **FR-008**: 相关单元测试与回归测试 MUST 覆盖 owner 收口与边界约束。  
- **FR-009**: 模块 README/CMake/module.yaml MUST 与真实实现边界一致。  

### Key Entities *(include if feature involves data)*

- **MotionPlannerOwnerAsset**: M7 内部唯一规划核心实现资产，包含输入 `ProcessPath + MotionConfig` 与输出 `MotionTrajectory` 的求解语义。  
- **RuntimeControlOrchestrationAsset**: M9 执行链控制资产，包含回零、点动、控制、状态与 IO 协调能力。  
- **BoundaryComplianceEvidence**: 边界门禁与测试报告资产，用于判定依赖/构建/职责是否合规。  
- **CMPPlanningBoundaryContract**: CMP 规划事实与执行触发包装分界的契约描述资产。  

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: `assert-module-boundary-bridges` 对 M6/M7/M9 扫描结果为 0 个新增越界项。  
- **SC-002**: `modules/motion-planning/tests` 与 `modules/runtime-execution/runtime/host/tests` 相关用例在 CI 配置下通过率 100%。  
- **SC-003**: `test.ps1 -Profile CI -Suite contracts` 在本特性分支上通过，无新增 known failure。  
- **SC-004**: `run-local-validation-gate.ps1` 产出完整报告且 `overall_status=passed`。  
- **SC-005**: 审核者可从 README/CMake/module.yaml 在 5 分钟内定位 M7/M9 的 owner 边界，无歧义。  
