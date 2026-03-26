# Feature Specification: 工作区模板化架构重构

**Feature Branch**: `refactor/arch/NOISSUE-architecture-refactor-spec`  
**Created**: 2026-03-24  
**Status**: Draft  
**Input**: 以 `docs/architecture/dsp-e2e-spec/` 作为唯一正式冻结文档集与架构对齐基线，对当前工作区执行目录、代码、脚本、测试、样本与文档的原地架构重构

## Clarifications

### Session 2026-03-24

- Q: 本轮重构是否要求将参考模板术语设为唯一规范名？ → A: 参考模板术语成为唯一规范名，旧名只允许在迁移说明中保留一次。
- Q: 本轮规范是否覆盖整个工作区？ → A: 全工作区都纳入本轮规范；主链模块必须完成对齐，非主链模块至少完成归属、边界和去留决策。
- Q: 本轮是否必须产出与参考模板 9 个轴一一对应的正式文档集？ → A: 必须产出与参考模板 9 个轴一一对应的完整正式文档集。
- Q: 本特性交付是否同时要求文档冻结与实际工作区结构/边界对齐？ → A: 本特性必须同时完成正式文档集和实际工作区结构/边界对齐，文档与仓库都要收敛。
- Q: 本轮业务逻辑调整应如何约束？ → A: 允许业务逻辑调整，但只能按模板对齐做受控纠偏，不允许借本轮做无边界的业务重定义。
- Q: 用户故事数量是否固定为 3 个？ → A: 不固定，必须优先服从独立验收边界、迁移风险隔离和波次依赖顺序。
- Q: `specs/refactor/arch/NOISSUE-architecture-refactor-spec/` 是否是本次任务的正式 spec 源？ → A: 否，该目录只作为既有参考；本次任务的正式 spec/plan/tasks 统一落在 `specs/refactor/arch/NOISSUE-workspace-template-refactor/`。

## User Scenarios & Testing *(mandatory)*

### User Story 1 - 锁定统一架构冻结基线 (Priority: P1)

作为架构负责人，我需要先锁定统一的阶段链、失败分层、评审入口和规范术语，这样后续所有目录迁移、模块拆分和验收讨论都基于同一套冻结基线进行，而不是继续引用旧文档、旧命名和隐式共识。

**Why this priority**: 如果没有统一冻结基线，后续波次会在不同人对阶段、术语、成败判定的不同理解上并行推进，最终必然产生二次返工和口径漂移。

**Independent Test**: 仅通过正式冻结文档和评审入口即可验证；当评审者能够沿统一术语从任务发起回链到执行归档，并在关键节点看到唯一阶段职责、成功条件、失败条件和最低追溯字段时，该故事即成立。

**Acceptance Scenarios**:

1. **Given** 当前工作区存在多套历史命名和不一致的架构说明，**When** 评审者依据新冻结文档审查端到端主链，**Then** 所有在范围链路都能映射到唯一阶段和统一验收口径。
2. **Given** 团队需要判断某个关键能力属于哪个阶段或失败层级，**When** 回查正式冻结文档，**Then** 无需额外口头解释即可找到唯一结论。

---

### User Story 2 - 固化 canonical roots 与共享支撑面 (Priority: P2)

作为工作区治理负责人，我需要让 `shared/`、`scripts/`、`samples/`、`tests/`、`deploy/` 等 canonical roots 成为正式承载面，并明确 `packages/`、`integration/`、`tools/`、`examples/` 只是迁移来源，这样后续模块迁移不会继续写回旧根。

**Why this priority**: 如果 canonical roots 只是占位目录，后续任何 owner 迁移都会退化为“文档写新根、实现仍在旧根”的双事实源状态。

**Independent Test**: 仅检查 canonical-paths、directory-responsibilities、共享层/脚本层/样本层说明和根级门禁，即可判断工作区是否已经建立了可承接后续迁移波次的正式根。

**Acceptance Scenarios**:

1. **Given** 团队准备迁移一类跨模块稳定资产，**When** 查看 canonical roots 与 shared/script/test support 规范，**Then** 能明确知道目标根、禁止回流规则和过渡桥的退出条件。
2. **Given** 历史路径仍然存在，**When** 评审者检查新规范与门禁，**Then** 能判断这些路径是否只剩迁移来源或兼容壳角色，而不是继续作为默认 owner 面。

---

### User Story 3 - 收敛上游静态工程链 `M1-M3` (Priority: P3)

作为上游工程链负责人，我需要先把 `job-ingest`、`dxf-geometry`、`topology-feature` 三个静态模块收敛到 `modules/`，并把相关样本、契约和测试归位到 canonical roots，这样后续规划链和运行时链才能建立在稳定上游事实上。

**Why this priority**: `M1-M3` 是规划链的输入事实源，若上游仍停留在旧根或边界不清，后续任何 owner 拆分都会建立在漂移输入之上。

**Independent Test**: 仅检查 `modules/job-ingest`、`modules/dxf-geometry`、`modules/topology-feature` 以及对应的样本/验证入口，就能确认上游输入事实和 owner 落位已经收敛。

**Acceptance Scenarios**:

1. **Given** 一个文件接入和几何构建链路，**When** 评审者检查模块 owner 和样本归位，**Then** 能明确看到接入、几何、拓扑三类事实的唯一 owner 和消费边界。
2. **Given** 历史 `engineering-*` 路径仍保留过渡材料，**When** 评审者检查迁移状态，**Then** 能确认它们不再作为 `M1-M3` 的终态 owner 面。

---

### User Story 4 - 收敛工作流与规划链 `M0`、`M4-M8` (Priority: P4)

作为流程与规划链负责人，我需要把 `workflow`、`process-planning`、`coordinate-alignment`、`process-path`、`motion-planning`、`dispense-packaging` 从 `process-runtime-core` 的混合承载面中拆出来，形成清晰的 owner 边界和依赖方向。

**Why this priority**: `process-runtime-core` 目前同时承载多个 owner，会直接掩盖规划链中最关键的事实归属和越权回写问题。

**Independent Test**: 仅检查对应 `modules/` 子树、owner 清单和依赖规则，就能判断 `M0/M4-M8` 是否已经成为真实模块 owner，而不是旧包语义的机械平移。

**Acceptance Scenarios**:

1. **Given** 一个从工作流到执行包构建的离线链路，**When** 评审者检查模块边界，**Then** 能逐段确认各事实对象在 `M0/M4-M8` 的唯一 owner 与允许依赖方向。
2. **Given** 历史 `process-runtime-core` 仍存在兼容壳，**When** 评审者检查切换状态，**Then** 能确认其不再承担 `M0/M4-M8` 的终态 owner 责任。

---

### User Story 5 - 收敛运行时执行链 `M9` 与运行时入口 (Priority: P5)

作为运行时负责人，我需要把 `runtime-execution`、`runtime-service`、`runtime-gateway` 以及相关设备契约/适配从旧宿主与网关路径中收敛出来，使运行时链有独立 owner 且不回写上游规划事实。

**Why this priority**: 运行时链是实时性和设备风险最高的部分，必须在规划链稳定后单独迁移，避免把“结构重组问题”和“执行语义回归问题”混在一起。

**Independent Test**: 仅检查 `modules/runtime-execution`、`apps/runtime-service`、`apps/runtime-gateway` 及相关控制语义，即可判断运行时 owner、入口职责和设备侧依赖边界是否已经收敛。

**Acceptance Scenarios**:

1. **Given** 一个离线规划已完成但进入运行态的链路，**When** 评审者检查运行时模块边界，**Then** 能确认运行时只消费上游结果，而不再回写规划对象。
2. **Given** 历史 `control-runtime`、`control-tcp-server`、`runtime-host`、`transport-gateway` 仍保留兼容路径，**When** 评审者检查切换状态，**Then** 能确认其职责已降级为入口壳或过渡桥。

---

### User Story 6 - 收敛追溯、HMI 与验证承载面 `M10-M11` (Priority: P6)

作为测试与交付负责人，我需要把 `trace-diagnostics`、`hmi-application`、仓库级 `tests/` 和稳定样本 `samples/` 收敛到 canonical roots，这样追溯、展示、审批和验收资产都有清晰的终态承载面。

**Why this priority**: 如果追溯/HMI/验证面仍与旧根混写，最终验收将无法证明“仓库结构、控制语义和验收证据”已经真正闭环。

**Independent Test**: 仅检查 `modules/trace-diagnostics`、`modules/hmi-application`、`tests/`、`samples/` 及其说明文档，即可判断追溯、展示、验证和样本是否都已落到正式承载面。

**Acceptance Scenarios**:

1. **Given** 一个需要从执行结果回链到测试证据和追溯对象的场景，**When** 评审者检查 trace/hmi/validation 结构，**Then** 能定位正式 owner、验证入口和样本来源，而不再依赖 `integration/` 或 `examples/`。
2. **Given** HMI 和 trace 仍存在历史实现痕迹，**When** 评审者检查迁移状态，**Then** 能确认新根已经承接正式职责，旧根只剩过渡角色。

---

### User Story 7 - 完成根级构建图切换与 legacy exit (Priority: P7)

作为发布与平台负责人，我需要在前面波次稳定后，把根级构建图切换到 canonical roots，并让 `packages/`、`integration/`、`tools/`、`examples/` 退出默认 owner 地位，这样根级 `build.ps1`、`test.ps1`、`ci.ps1` 和本地验证门禁才能基于终态结构运行。

**Why this priority**: 如果不完成根级切换和 legacy exit，前面的 owner 迁移只能算“局部存在”，无法证明工作区已经真正收敛到模板化终态。

**Independent Test**: 仅运行根级 `build.ps1`、`test.ps1`、`ci.ps1`、local validation gate，并审查 legacy exit 报告，即可判断 canonical roots 是否已经成为唯一真实构建图。

**Acceptance Scenarios**:

1. **Given** 所有 owner 模块和承载面已迁入 canonical roots，**When** 根级构建图切换完成，**Then** 根级入口应以 canonical roots 为默认真实实现来源并通过门禁。
2. **Given** 旧根仍然存在 tombstone 或 wrapper，**When** 评审者检查 legacy exit 结果，**Then** 能确认这些旧根不再承载终态 owner 逻辑。

## Edge Cases

- 当某项共享资产既像公共基础设施又带有明确业务 owner 时，必须优先判定 owner 模块，禁止为图省事而下沉到 `shared/`。
- 当历史路径必须在过渡期保留时，必须明确其角色是 wrapper、forwarder 还是 tombstone，并写明退出波次。
- 当部分模块已经迁入 canonical roots 而依赖方尚未切换时，必须保证桥接层显式、可审计且不可长期存在。
- 当某个模块无法自然映射到模板中的单一阶段或单一 owner 时，必须给出拆分、合并或冻结原边界的依据，禁止保留灰区。
- 当历史实现中同一事实对象由多个模块创建或修改时，必须判定唯一事实 owner，并禁止迁移后继续保留多事实源。
- 当运行时链迁移可能影响设备语义或实时性时，必须单独隔离波次，禁止与静态规划链一起总切换。
- 当 `tests/`、`samples/`、`scripts/` 的正式承载面已经可用时，不允许新增验证、样本或自动化逻辑继续写回 `integration/`、`examples/`、`tools/`。
- 当历史任务目录与本次特性目录同时存在时，本次任务的正式事实源只允许使用 `specs/refactor/arch/NOISSUE-workspace-template-refactor/` 下的产物；旧目录仅作为参考。

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: 系统必须产出一套面向当前工作区的统一架构规范，并以 `docs/architecture/dsp-e2e-spec` 的整套冻结文档作为参考模板来源。
- **FR-002**: 系统必须把当前工作区的端到端主链路映射到统一阶段链，并明确每个在范围阶段的输入、输出、成功判定、失败判定和最低追溯字段。
- **FR-003**: 系统必须定义覆盖当前工作区核心业务对象的统一对象清单，明确每个对象的业务含义、生命周期、关键关系和上下游引用约束。
- **FR-004**: 系统必须为每个核心对象指定唯一事实 owner，并明确允许的消费者、禁止的越权改写行为以及失败归责停留层级。
- **FR-005**: 系统必须为跨模块协作建立统一的接口契约基线，使阶段上下文、对象引用、版本语义、失败归因和追溯主键在跨模块交互中保持一致。
- **FR-006**: 系统必须明确当前工作区与参考模板在阶段链、对象链、模块边界、状态语义、失败分层、测试基线和仓库组织上的差距，并为每项差距给出唯一处置决策。
- **FR-007**: 系统必须定义统一的控制语义，至少覆盖状态机主链、关键命令、关键事件、阻断语义、回退语义、恢复语义和归档语义。
- **FR-008**: 系统必须显式区分离线规划/离线校验通过、执行门禁通过、首件放行通过和正式执行完成，禁止将这些结论互相替代或混写。
- **FR-009**: 系统必须建立统一的失败分层与回退规则，使所有关键失败都能映射到阶段、失败类别、推荐动作和回退目标。
- **FR-010**: 系统必须定义主成功链、阻断链、回退链、恢复链和归档链的系统协作口径，确保跨模块时序能够被稳定描述和审查。
- **FR-011**: 系统必须给出符合参考模板的工作区组织基线，明确应用入口、领域模块、共享层、数据资产、测试资产和工具资产的归位原则。
- **FR-012**: 系统必须建立覆盖对象级、阶段级、模块级、链路级、故障注入级、回归级和验收级的测试与验收基线，并要求每条关键用例都能回链到阶段、对象、状态、事件和失败码。
- **FR-013**: 系统必须保证重构后的规范仍能保留业务闭环所需的关键关联关系，包括任务、源文件、工艺语义、执行准备、运行记录、故障记录和归档追溯之间的关联。
- **FR-014**: 系统必须定义重构完成判定，使团队能够基于文档一致性、边界闭合度、状态闭合度、差距清零度和验收可执行性判断是否达到下一阶段。
- **FR-015**: 系统必须明确本轮重构范围边界，禁止把与架构规范冻结无直接关系的性能调优、单次故障排障、界面视觉改版、硬件整定或认证活动混入本特性范围。
- **FR-016**: 系统必须以参考模板中的阶段名、对象名、模块名、状态名和关键事件名作为唯一规范术语；历史术语仅允许在迁移映射说明中保留一次，之后不得继续作为正式别名并存。
- **FR-017**: 系统必须将整个工作区纳入本轮架构规范范围；端到端主链直接相关模块必须完成模板对齐，非主链模块至少必须完成归属判定、边界说明以及保留、迁移、合并、冻结或移除中的一种去留决策。
- **FR-018**: 系统必须产出与参考模板冻结轴一一对应的完整正式文档集，并保证阶段链、对象链、模块链、状态机、时序、失败与回退、测试与验收、仓库组织之间不存在缺轴或合并后失真的情况。
- **FR-019**: 系统必须将正式文档冻结与实际工作区结构/边界收敛作为同一特性的共同交付目标；任何已冻结的模块边界、目录归属、对象 owner 和协作契约都必须在仓库现实中得到对应落位，不允许长期停留在“文档已改、仓库未改”的分离状态。
- **FR-020**: 系统可以调整现有业务逻辑，但每一项调整都必须由参考模板对齐、边界闭合、失败分层修正或验收闭环直接驱动，并附带明确的依据、影响范围和回归验证点；不得在本特性中引入与模板对齐无直接关系的开放式业务重定义。
- **FR-021**: 系统必须让 `shared/`、`samples/`、`tests/`、`scripts/`、`deploy/` 成为真实 canonical roots，而不是长期停留在占位或桥接状态。
- **FR-022**: 系统必须为 `Wave 0-6` 定义明确的入口条件、退出条件、交付件清单和模块 cutover 判定标准。
- **FR-023**: 系统必须在前置波次验收通过后，将根级构建图切换到 canonical roots，并把 `packages/`、`integration/`、`tools/`、`examples/` 降级为 wrapper、tombstone 或移除状态。

### Scope Boundaries

- 本特性的范围包括：阶段链对齐、对象链对齐、模块边界对齐、状态与命令/事件语义对齐、失败与回退规则对齐、验收基线对齐，以及这些内容在工作区组织上的统一冻结与真实落位。
- 本特性要求最终交付完整的正式文档集，而不是只提交单篇总览文档或若干映射说明。
- 本特性要求覆盖整个工作区，而不是只覆盖主链代码目录；其中主链模块需要完成完整对齐，非主链模块至少需要完成归属、边界和去留决策。
- 本特性要求文档冻结、目录结构、代码归位、测试归位、样本归位和脚本归位同步推进，不接受只交付目标文档而把实际仓库改造整体后移的完成口径。
- 本特性允许对现有业务逻辑做受控纠偏，但这些调整必须能被参考模板、差距项或验收缺口直接证明其必要性；不接受借本轮进行无边界的业务规则重设计。
- 本特性不包括：单点性能优化、现场设备参数整定、临时故障排查、纯界面样式调整、与端到端流程规范无直接关联的仓库清理。
- `specs/refactor/arch/NOISSUE-architecture-refactor-spec/` 只作为历史参考，不是本特性的正式事实源。

### Assumptions & Dependencies

- **Assumption A1**: `docs/architecture/dsp-e2e-spec` 目录中的文档集被视为本轮工作区架构重构的参考模板与统一口径来源。
- **Assumption A2**: `docs/architecture/dsp-e2e-spec/` 是当前工作区唯一正式冻结事实源。
- **Assumption A3**: 当前工作区的核心业务闭环仍可抽象为“任务建立、文件接入、规划生成、执行准备、执行控制、归档追溯”的端到端链路。
- **Assumption A4**: 现有架构文档、模块划分、测试资产和历史交付记录可以作为现状证据使用，但不自动等于目标结构本身。
- **Dependency D1**: 需要能够访问当前工作区的现状目录、文档、模块边界和测试资产，以完成差距识别和责任归位。
- **Dependency D2**: 需要架构、业务和测试三类评审角色对统一阶段命名、对象命名、状态语义和验收口径达成一致，否则无法宣告规范冻结。
- **Dependency D3**: 需要参考模板在本轮期间保持稳定基线，否则当前工作区的目标结构会持续漂移。
- **Dependency D4**: 需要每个模板轴都能在当前工作区中找到对应事实来源或差距决策依据，否则无法形成完整的正式文档集。
- **Dependency D5**: 需要根级 `build.ps1`、`test.ps1`、`ci.ps1` 和 local validation gate 能够持续工作，以支撑每个波次的验收与 closeout。

### Key Entities *(include if feature involves data)*

- **Architecture Baseline**: 描述工作区目标架构的冻结基线，覆盖阶段链、对象链、模块链、控制语义和验收语义之间的一致关系。
- **Stage Definition**: 描述一个业务阶段应承担的职责、输入输出、成功条件、失败条件和最低追溯要求。
- **Artifact Definition**: 描述端到端流程中的核心事实对象、生命周期、引用关系、版本语义和唯一 owner 责任。
- **Module Contract**: 描述一个模块的职责边界、可生成对象、可消费对象、可发布或消费的关键信号，以及禁止的越权行为。
- **Canonical Root**: 描述工作区中允许承载正式实现、验证、样本、脚本或部署事实的根目录类别及其禁止事项。
- **Migration Wave**: 描述一次受控迁移批次的范围、前置依赖、交付件、入口条件、退出条件和 bridge 关闭标准。
- **Legacy Bridge**: 描述为保护迁移过程而保留的 wrapper、forwarder、compat CMake 或 tombstone，其存在必须有明确退出波次。
- **Acceptance Baseline**: 描述测试分层、关键场景、通过标准、阻断标准、回链字段和可复核证据。
- **Gap Resolution Item**: 描述当前工作区与参考模板之间的一条差距、其影响范围、处理决定、责任归属和完成判定。

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% 的在范围架构维度都能映射到参考模板的对应冻结文档或其统一主轴，不再存在无法归属的关键能力描述。
- **SC-002**: 100% 的在范围业务阶段都具备唯一阶段归属，并且每个阶段都具备输入、输出、成功判定、失败判定和最低追溯字段。
- **SC-003**: 100% 的在范围核心事实对象都具备唯一 owner、明确消费者边界和禁止越权规则，不再存在未决的多事实源冲突。
- **SC-004**: 100% 的关键场景都具备明确的验收基线，至少覆盖主成功链、设备阻断链、首件拒绝链、运行恢复或终止链和归档追溯链。
- **SC-005**: 100% 的已识别差距都被分类并给出唯一处理决定，且每项决定都附带责任归属和完成判定，不存在“已识别但无人接手”的架构悬空项。
- **SC-006**: 架构、业务和测试三类评审角色均能在一次评审中各自独立完成至少 1 条主成功链、1 条阻断链和 1 条回退或恢复链的回链检查，且不出现阻断级口径冲突。
- **SC-007**: 100% 的参考模板轴都在当前工作区形成一一对应的正式交付物，且正式评审中不存在因文档缺失、维度合并或口径失真导致的阻断项。
- **SC-008**: 100% 的已冻结模块边界、目录归属和对象 owner 决策都能在工作区实际结构中找到对应落位，不存在“文档目标已定义但仓库现实仍停留旧边界”的未闭环阻断项。
- **SC-009**: 100% 的业务逻辑调整项都能回链到参考模板要求、差距项或验收缺口，并具有对应的回归验证点；正式评审中不存在“已改行为无法说明为何必须调整”的阻断项。
- **SC-010**: `Wave 0-6` 全部具备明确交付件、入口门禁、退出门禁和证据路径，且不存在未定义关闭标准的迁移波次。
- **SC-011**: 根级 `build.ps1`、`test.ps1`、`ci.ps1` 与 local validation gate 在 canonical graph 上全部通过，不需要依赖隐式 legacy fallback。
- **SC-012**: `packages/`、`integration/`、`tools/`、`examples/` 不再作为终态 owner 面存在；若仍保留，则仅允许以 wrapper、tombstone 或显式迁移说明形式存在。
