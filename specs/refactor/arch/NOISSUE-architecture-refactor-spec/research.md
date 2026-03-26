# Phase 0 Research: 工作区架构模板对齐重构

## Decision 1: 正式文档集落位到单一 canonical 目录 `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\`

- Decision: 以参考模板 `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\` 的 S01-S09 为一一对应源，在 `docs\architecture\dsp-e2e-spec\` 下生成当前工作区的 9 轴正式文档；S10 作为冻结目录索引陪伴文档保留，不与 9 轴合并。
- Rationale: 当前特性要求整个工作区形成一套可冻结、可审查、可追责的正式文档集。单目录可以保证术语、索引、版本、评审入口和迁移映射统一，不把 9 轴拆散到多处维护。
- Alternatives considered:
  - 直接复用 `docs\architecture\dsp-e2e-spec\` 作为当前工作区冻结文档集：拒绝。该目录是参考模板，不应与当前工作区事实产物混写。
  - 把 9 轴分散到 `docs\process-model\`、`docs\validation\`、`docs\machine-model\`：拒绝。会削弱一一对应关系并增加跨文档追溯成本。

## Decision 2: 不引入新的顶级 `modules\` 根，采用现有 canonical monorepo 根承接模板模块

- Decision: 参考模板中的 M0-M11 仅作为目标模块语义模型，不在仓库根新建 `modules\`。实际仓库继续以 `apps\`、`packages\`、`integration\`、`tools\`、`docs\`、`config\`、`data\`、`examples\` 为 canonical 根。
- Rationale: 宪章明确要求新工作优先落在 canonical workspace roots。当前仓库已经完成根级 monorepo 骨架建设，新增 `modules\` 会制造第二套默认结构并直接违反路径治理原则。
- Alternatives considered:
  - 完整照搬模板目录，新增 `modules\workflow` 等根级目录：拒绝。会与既有 `packages\process-runtime-core`、`packages\runtime-host` 等 canonical owner 冲突。
  - 保持当前混合状态但不形成显式映射：拒绝。无法满足 FR-006、FR-017、FR-019 的边界与去留决策要求。

## Decision 3: 技术与验证基线以根级入口为唯一正式执行链

- Decision: 规划与后续实现一律以 `D:\Projects\SS-dispense-align\build.ps1`、`test.ps1`、`ci.ps1` 为正式 build/test/CI 入口，`tools\scripts\run-local-validation-gate.ps1` 作为本地门禁补充，包内脚本和 HIL/SIL 入口只作为特定轴的证据，不作为替代主入口。
- Rationale: 宪章要求通过根级入口验证。现有仓库已经把 control apps、packages、integration、protocol-compatibility、simulation 收口到根级脚本和 local gate，继续用分散入口会破坏可复现性。
- Alternatives considered:
  - 仅以包内测试脚本为主，根级脚本作为附属说明：拒绝。与当前治理方向相反。
  - 直接以 HIL 或 simulated-line 作为默认冻结门禁：拒绝。环境依赖过强，不适合作为所有实现任务的基础门禁。

## Decision 4: 当前工作区的模块对齐采用“语义模块 -> canonical path cluster”映射

- Decision: 参考模板 M0-M11 先映射到现有 canonical path cluster，再在冻结文档中给出 owner、禁止越权和后续拆分规则。

| 参考模块 | 当前 canonical path cluster | 说明 |
|---|---|---|
| M0 workflow | `packages\process-runtime-core\src\application` | 当前没有独立 workflow package，顶层编排先以 application 层承接 |
| M1 job-ingest | `apps\hmi-app` + `apps\control-cli` + `packages\application-contracts` + `packages\process-runtime-core\src\application` | 入口面、命令契约、任务上下文编排共同组成任务/文件接入层 |
| M2 dxf-geometry | `packages\engineering-data` + `packages\engineering-contracts` | DXF 解析、规范几何与工程协议的 canonical owner |
| M3 topology-feature | `packages\engineering-data` | 当前仍与离线工程预处理同包收口，冻结时需要显式标识内部子边界 |
| M4 process-planning | `packages\process-runtime-core\planning` + `packages\process-runtime-core\src\domain` | 工艺规划规则和对象 owner |
| M5 coordinate-alignment | `packages\process-runtime-core\planning` + `packages\process-runtime-core\machine` | 坐标链、补偿与“not_required”语义需在冻结文档中单列 |
| M6 process-path | `packages\process-runtime-core\planning` | 路径排序和工艺路径事实 |
| M7 motion-planning | `packages\process-runtime-core\src\domain\motion` + `packages\simulation-engine`(consumer) | 生产 owner 在 runtime core；simulation 只消费/验证 |
| M8 dispense-packaging | `packages\process-runtime-core\src\domain\dispensing` + `packages\process-runtime-core\src\application\usecases\dispensing` | 点胶时序、执行包与离线校验 |
| M9 runtime-execution | `packages\runtime-host` + `packages\device-contracts` + `packages\device-adapters` + `apps\control-runtime` + `apps\control-tcp-server` | 预检、首件、执行、故障恢复和设备交互 |
| M10 trace-diagnostics | `packages\traceability-observability` + `integration\` + `docs\validation\` | trace owner 在 package，集成脚本和验证文档负责证据链 |
| M11 hmi-application | `apps\hmi-app` + `apps\control-cli` + `packages\application-contracts` | 命令入口、审批、展示与应用查询，不得成为事实源 |

- Rationale: 当前仓库已形成清晰的 app/package 分层，但仍存在中间态。用 path cluster 表达当前落位，可以在不虚构“已存在独立模块”的前提下冻结 owner 与边界。
- Alternatives considered:
  - 直接假设每个 Mx 已有 1:1 包目录：拒绝。与当前事实不符。
  - 完全放弃 M0-M11，只保留现有路径名：拒绝。会丢失模板对齐目标。

## Decision 5: Compatibility/legacy 政策采用“显式迁移映射 + 非默认入口禁回流”

- Decision: 历史术语、历史路径、历史 fallback 行为只允许出现在迁移映射、归档材料或 deprecation 证据中一次，不允许继续作为正式别名并存。`shared\`、`third_party\`、`build\`、`logs\`、`uploads\`、`tests\`、`cmake\` 被归类为 support/generated/vendor roots，不作为新的默认 owner surface。
- Rationale: 特性要求文档冻结与仓库现实同步收敛。若继续默认接受 legacy 术语或 fallback，冻结文档无法形成唯一规范名，仓库边界也无法真正收口。
- Alternatives considered:
  - 保留双术语长期共存：拒绝。与 FR-016 冲突。
  - 将 support/vendor 目录直接并入新的 canonical 业务边界：拒绝。会模糊治理和实现 owner。

## Decision 6: 关闭条件以“文档完整度 + 路径收敛度 + 验证证据”三联闭环定义

- Decision: 本特性的完成判定同时要求三项成立:
  1. 9 轴正式文档和 1 份冻结目录索引完整落位。
  2. 所有主链模块和非主链模块都有明确 owner、边界和去留决策。
  3. 根级 build/test/CI 入口与必要的集成证据能证明 canonical 路径没有回流 legacy 默认行为。
- Rationale: 仅有文档无法证明仓库已经收敛，只有目录调整也无法证明口径冻结。三联闭环与 FR-018、FR-019、SC-007、SC-008 一致。
- Alternatives considered:
  - 只以文档交付为完成标准：拒绝。会留下“文档已改、仓库未改”的分离状态。
  - 只以目录改造为完成标准：拒绝。无法形成跨角色统一语言和验收基线。
