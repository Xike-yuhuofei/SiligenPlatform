# Feature Specification: ARCH-203 Process Model Owner Boundary Repair

**Feature Branch**: `refactor/process/ARCH-203-process-model-owner-repair`  
**Created**: 2026-03-31  
**Status**: Draft  
**Input**: User description: "基于 2026-03-31 的九份模块架构审查结果，收口 process model 主链各模块的 owner 边界、public surface、兼容壳与构建文档测试一致性"

## Review Baseline

- 本特性以 2026-03-31 产出的 9 份模块架构审查为事实输入，覆盖 `workflow`、`topology-feature`、`process-planning`、`coordinate-alignment`、`process-path`、`motion-planning`、`dispense-packaging`、`runtime-execution`、`hmi-application`。
- 本特性同时采用同日复核报告作为纠偏基线；后续修复决策必须以复核后的事实为准，尤其不得继续沿用已被复核报告纠正的 `dispense-packaging` 与 `runtime-execution` 依赖归因表述。
- 本特性聚焦“模块 owner 收口”，不是重新发明业务流程；允许的行为调整仅限于为边界闭合、责任归位和验收一致性所必需的受控纠偏。

## Stage Scope

- **目标**: 让 process model 主链形成可执行的唯一 owner 地图，使各模块只通过声明的公共边界协作，而不是继续依赖 `workflow` 内部路径、宿主实现或历史兼容壳。
- **在范围模块**: `workflow (M0)`、`topology-feature (M3)`、`process-planning (M4)`、`coordinate-alignment (M5)`、`process-path (M6)`、`motion-planning (M7)`、`dispense-packaging (M8)`、`runtime-execution (M9)`、`hmi-application (M11)`。
- **主线焦点**: owner artifact 唯一性、public surface 收口、重复实现处置、兼容壳退出条件、文档/构建/测试边界一致化。
- **本阶段不包含**: 与 owner 收口无直接关系的开放式业务重定义、性能优化、现场调机、界面样式改版、非评审主线模块的扩展开发。

## User Scenarios & Testing *(mandatory)*

### User Story 1 - 冻结唯一 owner 地图 (Priority: P1)

作为架构负责人，我需要把九个评审模块的职责、owner artifact、允许依赖和禁止依赖冻结成一张单一边界地图，这样团队在继续整改时能够先回答“谁拥有什么”，而不是继续在历史目录和兼容层里猜测真实 owner。

**Why this priority**: 如果没有唯一 owner 地图，任何后续迁移都会继续把新逻辑写回旧模块、宿主应用或兼容壳，导致整改范围持续扩散。

**Independent Test**: 仅检查本故事产物即可独立验证；当评审者针对任一主链事实对象或能力都能定位唯一 owner 模块、唯一公共交互面和明确的禁止依赖规则时，本故事即成立。

**Acceptance Scenarios**:

1. **Given** 任一在范围主链能力需要归属模块 owner，**When** 评审者查阅规范，**Then** 该能力必须映射到唯一模块 charter、唯一 owner artifact 和唯一公共边界。
2. **Given** 某个模块或宿主希望消费上游能力，**When** 团队依据规范设计接入方式，**Then** 只能通过声明的 contracts、facade 或应用面接入，而不能把内部路径、helper target 或宿主 concrete 实现当作稳定边界。

---

### User Story 2 - 收口主链 live owner 实现 (Priority: P1)

作为模块维护者，我需要把当前散落在 `workflow`、宿主应用和重复树中的 live owner 事实收回到各自的 canonical 模块，使规划链、执行链和 HMI 前置决策链都按唯一 owner 演进，而不是维持多源并存。

**Why this priority**: 这决定整改是否只是“文档说收口”，还是主链真的能摆脱重复实现、反向依赖和职责漂移。

**Independent Test**: 仅检查 in-scope 模块的 owner 归位和重复实现处置表即可独立验证；当每个被评审模块都能说明哪些事实必须迁入、哪些重复实现必须退化、哪些兼容层允许短期保留以及何时退出时，本故事即成立。

**Acceptance Scenarios**:

1. **Given** 主链对象在当前代码中存在双 owner、影子实现或兼容转发，**When** 完成本特性定义，**Then** 每一处都必须被标记为 canonical owner、只读兼容壳、临时 bridge 或待移除项中的唯一一种。
2. **Given** `workflow` 当前仍承载多个非 `M0` owner 事实，**When** 完成本特性定义，**Then** 规范必须明确哪些事实继续留在编排层、哪些必须迁回 `M3-M11` 模块，以及迁移后 `workflow` 不得继续吸收新的非编排 owner 逻辑。
3. **Given** `hmi-application`、`runtime-execution` 和规划链模块当前与宿主或旧模块存在边界混写，**When** 团队执行整改，**Then** 这些模块必须以自身 owner 决策和状态语义为中心，而宿主与外部消费者只保留调用、渲染或装配责任。

---

### User Story 3 - 统一消费者接入与验收证据 (Priority: P2)

作为测试与交付负责人，我需要让宿主应用、CLI、gateway、runtime host 以及评审文档、构建边界、测试入口全部指向同一套 canonical 模块边界，这样整改完成后可以被复核、验证和持续维护。

**Why this priority**: 如果消费者接入面和验证资产不一致，团队即使完成局部迁移，也无法证明“模块边界已经真正稳定”。

**Independent Test**: 仅检查消费者边界规则和验收证据矩阵即可独立验证；当所有在范围消费者都被要求改接 canonical public surface，且每个模块的文档、构建、测试和库存描述都对齐到同一边界时，本故事即成立。

**Acceptance Scenarios**:

1. **Given** 宿主、CLI 或运行时入口当前直接依赖模块内部路径或宿主 concrete 逻辑，**When** 团队依据规范整改，**Then** 这些接入点必须改为依赖对应模块声明的公共边界，并停止把内部实现当作长期契约。
2. **Given** 团队在整改后重新复核九个模块，**When** 审查文档、构建边界、测试入口和模块库存，**Then** 不应再出现“文档说已收口、构建仍指向旧树、测试仍绑定旧 owner”的三套口径并存。

### Edge Cases

- 当同一事实对象在多个模块中都存在实现或镜像类型时，必须指定唯一 canonical owner，其余位置只能被定义为只读兼容、临时过渡或移除目标，禁止继续并列演进。
- 当某个模块当前只有骨架而缺少 live owner 实现时，必须明确缺失 owner 事实应迁入何处、迁移前允许哪些临时 bridge 存在，以及什么条件代表该缺口已经关闭。
- 当历史兼容壳必须暂时保留以避免一次性断裂时，必须限制其不得承载新增业务逻辑，并为其记录明确退出信号和消费者切换条件。
- 当评审文档、复核报告与当前代码事实存在冲突时，必须以复核后的 current-fact 结论为准，并在整改边界中记录作废口径，避免继续围绕错误事实制定迁移计划。
- 当文档、构建、测试或模块库存中有任一维度仍指向旧 owner 边界时，模块不得被判定为“已收口完成”。

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: 本特性必须以九份模块架构审查和同日复核报告作为唯一事实输入，并要求后续整改决策全部回链到这些输入。
- **FR-002**: 本特性必须为每个在范围模块定义清晰的模块 charter，至少包含模块职责、唯一 owner artifact、允许消费者、允许依赖和禁止依赖。
- **FR-003**: 本特性必须为主链核心交接对象建立唯一 owner 映射，至少覆盖 `ProcessPlan`、`CoordinateTransformSet/AlignmentCompensation`、`ProcessPath`、`MotionPlan`、`Trigger/CMP packaging`、`ExecutionSession` 和 HMI 前置决策/会话状态。
- **FR-004**: 本特性必须把 `workflow` 收紧为编排与受控兼容责任，不得继续将其定义为 `M3-M11` 这些 owner 模块事实的长期 live implementation root。
- **FR-005**: 本特性必须明确 `runtime-execution` 的执行 owner 范围，并要求执行主流程、执行状态和执行契约不再长期依赖 `workflow` 承担 canonical owner 事实。
- **FR-006**: 本特性必须明确 `process-planning -> coordinate-alignment -> process-path -> motion-planning -> dispense-packaging -> runtime-execution` 这条主链的交接边界，并禁止依赖隐藏上下文或旧聚合根完成跨模块协作。
- **FR-007**: 本特性必须明确 `topology-feature` 的 owner 范围和外部消费边界，禁止其它模块或入口继续持有其 concrete adapter 作为稳定公开面。
- **FR-008**: 本特性必须明确 `hmi-application` 与宿主 HMI 应用的职责分层，使 owner 决策、会话状态和前置阻断语义归位到模块，而宿主只保留界面承载、事件转发和外部调用责任。
- **FR-009**: 本特性必须为每一处已识别的重复实现、影子实现、镜像类型或兼容壳给出唯一处置决定，处置类型只能是 canonical owner、只读兼容壳、临时 bridge 或移除目标之一。
- **FR-010**: 所有被保留的兼容壳都必须声明退出条件、消费者切换条件和禁止新增业务逻辑的约束，不允许无限期演化成新的 live owner 面。
- **FR-011**: 本特性必须建立统一 public surface 规则，要求在范围消费者只能依赖模块声明的 contracts、facade 或应用边界，不得把内部 domain 路径、helper target、私有头树或宿主 concrete 实现作为稳定接入点。
- **FR-012**: 本特性必须要求每个在范围模块的文档、构建边界、测试入口和模块库存描述对齐到同一 canonical owner 边界，不允许继续出现相互矛盾的边界口径。
- **FR-013**: 本特性必须把 `dispense-packaging` 评审中已被复核纠正的事实偏差纳入整改基线，并禁止后续修复再基于已作废的错误归因展开。
- **FR-014**: 本特性必须为每个在范围模块定义最小验收证据，至少覆盖 owner charter、交接对象、重复实现处置、消费者接入规则和边界一致性证明。
- **FR-015**: 本特性必须为整改执行定义统一的阶段顺序，至少包括边界冻结、owner 迁移、消费者改接、兼容壳清理和证据回写这五类动作。
- **FR-016**: 本特性必须明确每个在范围模块的优先级和波次归属，保证团队知道哪些问题必须先处理，哪些只能在上游 owner 收口后跟进。
- **FR-017**: 本特性必须把“文档已调整但仓库现实未收口”的状态视为未完成，并要求完成判定同时覆盖代码边界、公共交互面和验证资产。
- **FR-018**: 本特性必须将与 owner 收口无直接关系的性能、视觉、现场工艺整定或开放式业务规则重写排除在范围之外；任何行为调整都必须能证明其直接服务于边界闭合或验收一致性。

### Scope Boundaries

- 本特性范围包括九个评审模块及其主链消费者的 owner boundary repair，不是对整个仓库做一次新的总架构重命名。
- 本特性范围包括重复实现和兼容壳的处置决策，但不要求在同一阶段完成所有历史目录的物理删除；允许保留短期 bridge，前提是退出条件明确且不再承载新增逻辑。
- 本特性范围包括文档、构建边界、测试入口和模块库存的一致性回写，因为这些资产决定团队是否真的共享同一条边界事实。
- 本特性不包括未进入本次九份评审的问题域，除非这些问题域被证明是 in-scope 模块完成 owner 收口所必需的直接依赖。
- 本特性不包括以“顺手优化”为名的额外业务重设计；任何新增或调整行为都必须附带边界收口必要性和回归验证点。

### Assumptions & Dependencies

- **Assumption A1**: 2026-03-31 的九份模块审查和同日复核报告足以作为当前 owner boundary repair 的事实基线。
- **Assumption A2**: 本轮整改优先解决“模块拥有权和交互边界错误”，而不是优先解决所有实现细节质量问题。
- **Assumption A3**: 允许存在受控兼容窗口，但兼容窗口本身必须被视为过渡态，而不是新的长期结构。
- **Dependency D1**: 需要架构、模块维护和测试三类角色接受同一组 canonical owner 定义，否则整改会再次退化为局部视角修补。
- **Dependency D2**: 需要在范围消费者配合切换到 canonical public surface，否则兼容壳无法退出。
- **Dependency D3**: 需要文档、构建和测试资产可以同步回写；如果这些资产被冻结或长期滞后，边界收口将无法被验证。

### Key Entities *(include if feature involves data)*

- **Module Charter**: 某个模块被允许拥有的职责、对象和对外公共边界的正式定义。
- **Owner Artifact**: 某项业务事实或状态在仓库中的唯一 canonical owner 表示。
- **Handoff Artifact**: 主链上下游之间用于交接责任的稳定对象，例如 `ProcessPlan`、`ProcessPath`、`MotionPlan`、`ExecutionSession`。
- **Canonical Public Surface**: 外部消费者被允许稳定依赖的 contracts、facade 或应用边界。
- **Compatibility Shell**: 为保障迁移窗口而短期保留的只读兼容层；它不拥有业务事实，只负责过渡。
- **Shadow Implementation**: 与 canonical owner 重复、镜像或漂移的实现、类型或 adapter，需要被归位、退化或移除。
- **Boundary Evidence**: 证明模块边界已收口的一组证据，至少包括文档口径、消费者接入规则、验证入口和重复实现处置结果。

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% 的在范围模块都具备已冻结的模块 charter，且每个 charter 都包含唯一 owner artifact、允许消费者、允许依赖和禁止依赖。
- **SC-002**: 100% 的主链交接对象都具备唯一 canonical owner 与唯一公共交互面；未决的多 owner 冲突数量为 `0`。
- **SC-003**: 100% 的已识别重复实现、影子实现和兼容壳都获得唯一处置决定；未分类项数量为 `0`。
- **SC-004**: 在范围消费者中，被认可为长期稳定接入点的内部路径、helper target、宿主 concrete 实现依赖数量为 `0`。
- **SC-005**: 100% 的在范围模块都实现文档、构建边界、测试入口和模块库存描述的一致化；未登记的不一致项数量为 `0`。
- **SC-006**: 评审者能够基于规范独立回链至少 1 条规划到执行的主成功链和 1 条失败/恢复链，并且全程只使用 canonical 模块边界解释，不再出现 owner 歧义。
- **SC-007**: 100% 的被保留兼容壳都记录了退出条件和消费者切换条件；没有任何兼容壳被允许作为长期新增业务逻辑入口。
- **SC-008**: 所有整改决策都能回链到九份评审或复核报告；基于已被纠正事实继续立项的整改项数量为 `0`。
