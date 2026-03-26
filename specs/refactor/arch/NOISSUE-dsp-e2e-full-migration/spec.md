# Feature Specification: DSP E2E 全量迁移与 Legacy 清除

**Feature Branch**: `refactor/arch/NOISSUE-dsp-e2e-full-migration`  
**Created**: 2026-03-25  
**Status**: Draft  
**Input**: User description: "以 `docs/architecture/dsp-e2e-spec/` 为唯一正式冻结标准，完成整仓新架构现实收敛并物理清除旧架构资产"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - 统一正式冻结事实源 (Priority: P1)

作为架构治理负责人，我需要让 `docs/architecture/dsp-e2e-spec/` 成为整仓迁移的唯一正式冻结事实源，这样团队在判断模块边界、生命周期、状态语义、接口契约、时序和验收基线时，不会再受到历史说明、临时分析或 legacy 残留材料的干扰。

**Why this priority**: 如果正式事实源不唯一，后续所有“是否迁移完成”的判断都会出现双重口径，导致整仓收敛无法成立。

**Independent Test**: 任选 `dsp-e2e-spec-s05/s06` 定义的 `M0-M11` 中任一模块，并检查其在 `workspace-baseline` 定义的 `target canonical roots` 中的真实承载面；评审者仅依据本规格和 `dsp-e2e-spec` 冻结文档，就能确定目标 owner 面、完成判定和不合规情形；若仍需依赖历史材料补充裁决，则该故事未完成。

**Acceptance Scenarios**:

1. **Given** 当前仓库中存在历史说明、临时分析和已迁移但未收敛的目录结构，**When** 团队判定某模块的正式边界和完成标准，**Then** 只能以 `docs/architecture/dsp-e2e-spec/` 和本规格作为裁决依据。
2. **Given** 有人主张对部分模块使用较宽松的完成标准，**When** 团队执行本特性验收，**Then** 所有模块都必须按同一套整仓级完成标准接受判定，不允许出现例外口径。

---

### User Story 2 - 收敛真实承载与 owner 面 (Priority: P2)

作为模块负责人，我需要把 `dsp-e2e-spec-s05/s06` 定义的 `M0-M11` 模块，以及分布在 `apps/`、`modules/`、`shared/`、`tests/`、`samples/`、`scripts/` 等 `target canonical roots` 中与 owner 直接相关的真实实现、真实构建关系、测试、契约、样本和配套资产全部收敛到新架构规定的位置，使新架构目录从“统一骨架”变成“唯一真实 owner 面”。

**Why this priority**: 只完成目录搭壳或文档改名并不能消除旧架构语义；只有真实承载面完成迁入，仓库现实才算真正对齐。

**Independent Test**: 任选 `M0-M11` 中任一模块，并检查其在 `apps/`、`modules/`、`shared/`、`tests/`、`samples/`、`scripts/` 中的相关资产；仅检查仓库现实就能确认其真实实现面、真实构建面和 owner 面都已落在新架构规定位置，且不再依赖旧承载层提供事实或入口。

**Acceptance Scenarios**:

1. **Given** 某模块已经出现在新架构目录下，**When** 评审者检查其真实实现和真实构建入口，**Then** 这些事实必须直接落在新架构规定位置，而不是继续通过旧目录或兼容壳间接承载。
2. **Given** 某模块的测试、契约、样本或配套资产分布在 `apps/`、`shared/`、`tests/`、`samples/`、`scripts/` 等 `target canonical roots` 中，**When** 团队检查迁移结果，**Then** 这些资产必须与新 owner 面保持一致，不再保留旧语义组织方式作为终态依赖。

---

### User Story 3 - 物理清除 legacy 并完成最终验收 (Priority: P3)

作为发版和验收负责人，我需要在真实迁移完成后物理清除所有 legacy 承载层与兼容残留，并确认仓库现实与 `dsp-e2e-spec` 在模块边界、生命周期、状态语义、接口契约、时序和验收基线上保持一致，同时确认根级 `build.ps1`、`test.ps1`、`ci.ps1` 与 `scripts/validation/run-local-validation-gate.ps1` 只基于 canonical graph 运行且不依赖 legacy fallback。

**Why this priority**: 如果 legacy 只是失去正式地位却继续存在，团队仍会在维护和验收中回退到旧语义，整仓迁移无法宣告完成。

**Independent Test**: 对仓库进行整仓审查并运行根级 `build.ps1`、`test.ps1`、`ci.ps1` 与本地 gate 时，评审者找不到 bridge、forwarder、wrapper、兼容壳或 legacy 承载层作为终态组成部分，且所有入口都不依赖 legacy fallback，并能对任一模块完成“一致性回链”审查。

**Acceptance Scenarios**:

1. **Given** 根级 `build.ps1`、`test.ps1`、`ci.ps1` 或本地 gate 仍需通过 legacy fallback、compat CMake 或桥接目录解析真实目标，**When** 团队执行最终验收，**Then** 本特性必须判定为未完成。
2. **Given** 某 legacy 资产仅被降级为“参考”或“兼容层”但仍被仓库保留，**When** 团队执行最终验收，**Then** 本特性必须判定为未完成。
3. **Given** 某模块在文档上已经对齐新架构，但仓库现实仍保留旧边界、旧状态语义或旧契约入口，**When** 团队执行最终验收，**Then** 本特性必须判定为未完成，直到仓库现实同步收敛。

### Edge Cases

- 当模块表面上已经迁入新目录，但真实构建或依赖入口仍指向旧承载位置时，必须判定为未完成迁移。
- 当旧架构子树被嵌入新目录内部继续承载真实实现时，必须视为 legacy 残留，而不是视为“已迁移完成”。
- 当 bridge、forwarder、wrapper、兼容壳或 tombstone 仍作为真实入口存在时，必须视为终态不合规。
- 当 `apps/`、`shared/`、`tests/`、`samples/`、`scripts/` 中仍承载旧真实入口或旧 owner 语义时，必须视为主迁移对象，不得按“附属资产”降级处理。
- 当根级 `build.ps1`、`test.ps1`、`ci.ps1` 或本地 gate 仍通过 legacy fallback、compat CMake 或桥接目录解析真实目标时，必须判定为未完成迁移。
- 当同一模块的边界、生命周期或状态语义在冻结文档与仓库现实之间出现冲突时，必须先消除冲突后才能继续验收。
- 当某项资产无法明确归属到唯一新架构 owner 面时，必须先完成归位或删除决策，不允许以“暂存”状态进入终态。
- 当 `cmake/`、`third_party/`、`build/`、`logs/`、`uploads/`、正式冻结文档、`specs/` 特性工件或审计/验证报告不承载旧真实入口时，不得被误判为 legacy 清除对象。

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: 本特性必须将 `docs/architecture/dsp-e2e-spec/` 作为整仓迁移与完成判定的唯一正式冻结事实源。
- **FR-002**: 本特性必须以 `dsp-e2e-spec-s05/s06` 定义的 `M0-M11` 作为正式模块集合，并以 `workspace-baseline` 定义的 `target canonical roots` 作为正式目标根集合。
- **FR-003**: 本特性必须以整仓级口径覆盖 `M0-M11` 及其在 `apps/`、`modules/`、`shared/`、`docs/`、`samples/`、`tests/`、`scripts/`、`config/`、`data/`、`deploy/` 中的正式承载面，不得对任何模块采用“主链严格、非主链放宽”的双重完成标准。
- **FR-004**: 本特性必须识别并纳入所有与 `M0-M11` owner 直接相关的真实源码承载位置、真实构建定义与依赖入口、测试资产、契约资产、样本资产、脚本资产以及新旧架构切换相关的目录承载关系。
- **FR-005**: 每个 `M0-M11` 模块的真实实现必须整理并分布到新架构对应目录下，使新架构目录成为该模块的唯一真实实现面。
- **FR-006**: 每个 `M0-M11` 模块的真实构建定义与依赖入口必须收敛到新架构对应位置，使新架构目录成为该模块的唯一真实构建面。
- **FR-007**: 与 `M0-M11` owner 直接相关并分布在 `apps/`、`shared/`、`tests/`、`samples/`、`scripts/` 中的测试、契约、样本和配套资产必须与新架构 owner 面保持一致，不得继续依赖旧组织方式作为终态承载。
- **FR-008**: 本特性必须将新架构目录从“统一骨架或承载壳”收敛为“唯一真实 owner 面”，不得仅停留在结构占位或目录映射层面。
- **FR-009**: 本特性必须识别所有 legacy 资产，包括旧顶层目录、嵌入新目录内部的旧架构子树、bridge、forwarder、wrapper、tombstone、兼容 `CMakeLists.txt`、隐式 fallback 路径和其他兼容承载层，并将其纳入清除范围。
- **FR-010**: 所有已识别 legacy 资产都必须被物理清除，不得仅降级为“非正式但仍保留”的兼容层或参考层。
- **FR-011**: `cmake/`、`third_party/`、`build/`、`logs/`、`uploads/`、正式冻结文档、`specs/` 特性工件以及审计/验证报告，不属于默认 legacy 清除对象；只有在它们仍承载旧真实入口、旧真实构建面或旧 owner 事实时，才进入清除范围。
- **FR-012**: 根级 `build.ps1`、`test.ps1`、`ci.ps1` 与 `scripts/validation/run-local-validation-gate.ps1` 必须在 canonical graph 上运行，且不得依赖 legacy fallback、compat forwarder、桥接目录或旧承载路径才能通过。
- **FR-013**: 本特性必须产出一份权威的迁移归位与清除矩阵，逐项列出 `M0-M11` 及相关 canonical roots 资产的目标 owner 面、旧承载位置、迁移状态、清除状态和阻断项。
- **FR-014**: 本特性必须以仓库现实是否完成真实承载收敛作为新架构是否成立的判断依据，不得以“目标目录已存在”替代完成判定。
- **FR-015**: 只有当所有 `M0-M11` 模块同时满足真实实现面、真实构建面和真实 owner 面收敛，且根级正式入口零 legacy fallback 后，才能判定整仓迁移完成。
- **FR-016**: 本特性必须保证仓库现实与 `dsp-e2e-spec` 在模块边界、生命周期、状态语义、接口契约、系统时序和验收基线上保持一致。
- **FR-017**: 当冻结文档、历史说明和仓库现实之间存在冲突时，本特性必须以 `dsp-e2e-spec` 为最高裁决依据，并推动仓库现实完成对齐。
- **FR-018**: 本特性必须提供明确的不合规判定规则，至少覆盖“新目录存在但真实实现未迁入”“新目录存在但真实构建未迁入”“legacy 被嵌入新目录继续承载”“compatibility 壳仍为真实入口”“根级正式入口仍依赖 legacy fallback”这五类情形。
- **FR-019**: 本特性必须把物理清除 legacy 资产作为最终交付的一部分，而不是作为后续可选治理事项。

### Scope Boundaries

- 本特性的正式模块范围固定为 `M0-M11`；正式目标根范围固定为 `apps/`、`modules/`、`shared/`、`docs/`、`samples/`、`tests/`、`scripts/`、`config/`、`data/`、`deploy/`。
- 本特性的范围包括：`M0-M11` 的真实源码承载位置、真实构建与依赖入口、与模块 owner 直接相关并分布在 canonical roots 中的测试/契约/样本/脚本/配套资产，以及与新旧架构切换直接相关的目录承载关系和 legacy 残留。
- 本特性的完成判定对所有模块一视同仁，不存在按业务主次或链路重要性分层放宽的口径。
- 本特性不以新架构目录是否已经存在作为完成标志，而以仓库现实是否完成真实承载收敛、以及根级正式入口是否零 legacy fallback 作为完成标准。
- 本特性默认不将 `cmake/`、`third_party/`、`build/`、`logs/`、`uploads/`、正式冻结文档、`specs/` 特性工件和审计/验证报告视为清除对象，除非这些资产仍承载旧真实入口、旧真实构建面或旧 owner 事实。
- 本特性不包括：仅做文档改名、索引调整或术语替换；仅完成骨架搭建但继续把真实实现留在旧组织方式中；仅把旧架构降级为参考或兼容层而不做物理清除；借本轮迁移进行与架构收敛无直接关系的开放式业务重定义。

### Assumptions & Dependencies

- **Assumption A1**: `docs/architecture/dsp-e2e-spec/` 在本特性实施期间保持为唯一正式冻结文档集，不再并行维护第二套正式冻结口径。
- **Assumption A2**: 当前仓库中已存在可供审查的代码、构建、测试、契约和样本资产，足以完成整仓级现实收敛与差距识别。
- **Assumption A3**: 现有迁移状态是“部分迁移但未最终收敛”，因此本特性以现实对齐和 legacy 清除为核心，而不是从零设计新架构。
- **Assumption A4**: 根级 `build.ps1`、`test.ps1`、`ci.ps1` 与 `scripts/validation/run-local-validation-gate.ps1` 继续作为正式验收入口，不通过新增临时入口替代。
- **Dependency D1**: 需要能够对整个仓库进行模块边界、目录承载关系和资产归属审查，才能完成统一判定。
- **Dependency D2**: 需要各模块 owner 接受统一的最终完成标准，否则无法实现整仓级收敛。
- **Dependency D3**: 需要验收角色能够同时检查冻结文档与仓库现实的一致性，而不是只审查其中一侧。
- **Dependency D4**: 需要根级正式入口能够稳定证明 canonical graph 是否已成为唯一真实构建与验证图，否则无法完成零 fallback 验收。

### Key Entities *(include if feature involves data)*

- **Frozen Source**: `docs/architecture/dsp-e2e-spec/` 所定义的唯一正式冻结事实源，用于裁决边界、状态、契约、时序和验收口径。
- **Canonical Root Set**: `workspace-baseline` 定义的 `target canonical roots`，是 `M0-M11` 及相关资产的唯一正式目标根集合。
- **Module Owner Surface**: 某模块在新架构中的唯一真实 owner 面，承载该模块的真实实现、真实构建，以及分布在 canonical roots 中的直接相关资产归属。
- **Legacy Asset**: 旧架构承载层或兼容残留，包括旧目录、嵌入式旧子树、桥接层、转发层、包装层、墓碑层、兼容 `CMakeLists.txt`、隐式 fallback 路径和其他兼容壳；不默认包含未承载旧真实入口的支撑、文档、工件和证据资产。
- **Migration Gap**: 冻结目标与仓库现实之间的差异项，表现为真实承载位置、构建入口、资产归属或语义一致性未完成收敛。
- **Migration Alignment And Clearance Matrix**: 用于逐项证明 `M0-M11` 及相关 canonical roots 资产的目标 owner 面、旧承载位置、迁移状态、清除状态和阻断项的权威矩阵。
- **Completion Baseline**: 用于判断整仓迁移是否完成的一组统一条件，要求模块边界、真实 owner 面、根级正式入口零 fallback 和 legacy 清除同时满足。

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% 的 `M0-M11` 模块都能映射到新架构中的唯一真实实现面、唯一真实构建面和唯一真实 owner 面。
- **SC-002**: 100% 的 `M0-M11` 模块及其分布在 canonical roots 中的相关资产都按同一套整仓级完成标准接受验收，不存在任何例外模块、例外根或放宽口径。
- **SC-003**: 0 个 legacy 承载层或兼容残留保留在终态仓库中，包括 bridge、forwarder、wrapper、tombstone、旧顶层承载目录、嵌入式旧架构子树、兼容 `CMakeLists.txt` 和隐式 fallback 路径。
- **SC-004**: 100% 与模块 owner 直接相关并分布在 `apps/`、`shared/`、`tests/`、`samples/`、`scripts/` 中的测试、契约、样本和配套资产都完成与新架构 owner 面的一致归位，不再依赖旧组织方式作为正式入口。
- **SC-005**: 100% 的架构裁决争议都能通过 `dsp-e2e-spec` 与仓库现实的一致性检查得到唯一结论，不再需要历史说明作为补充正式依据。
- **SC-006**: 根级 `build.ps1`、`test.ps1`、`ci.ps1` 与 `scripts/validation/run-local-validation-gate.ps1` 在最终验收时全部通过，且 0 个入口依赖 legacy fallback、compat forwarder、桥接目录或旧承载路径。
- **SC-007**: 迁移归位与清除矩阵中 100% 的 `M0-M11` 和相关 canonical roots 资产条目都具备明确目标 owner 面、旧承载位置、迁移状态、清除状态和阻断结论，不存在未盘点条目。
- **SC-008**: 任意抽取一个 `M0-M11` 模块，评审者都能在 10 分钟内完成“边界、生命周期、状态语义、接口契约、时序、验收基线”一致性回链检查，并得到明确完成或未完成结论。
- **SC-009**: 最终验收时，0 个模块被判定为“仅骨架对齐”“文档已对齐但仓库未对齐”“通过兼容层维持迁移表象”或“根级入口仍靠 legacy fallback”的未完成状态。
